#include "agent-client-manager.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "log.h"
#include "errno.h"
#include "string.h"
#include "sys/epoll.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "netinet/tcp.h"
#include "arpa/inet.h"
#include "pthread.h"

void* acm_event_loop(void *arg);
int acm_hold_request(struct acm_channel *p_channel, 
        unsigned long req_id, 
        void (*listening)(void* arg, char *resp_data, int data_size), 
        void* arg);
void acm_response(void *arg);

/* 函数名: struct acm_handle* acm_start(struct acm_opt* p_opt) 
 * 功能: 启动manager
 * 参数: struct acm_opt* p_opt, 选项
 * 返回值: 
 */
struct acm_handle* acm_start(struct acm_opt* p_opt){
    struct acm_handle *p_new_handle;
    unsigned long tid;

    p_new_handle = malloc(sizeof(struct acm_handle));

    p_new_handle->max_write_queue_len = p_opt->max_write_queue_len;
    p_new_handle->max_hold_req_num = p_opt->max_hold_req_num;


    if (p_new_handle == NULL) {
        log_err("Failed to malloc memory for new acm_handle.");
        goto err1_ret;
    }

    log_info("ACM:Creating io thread pool.");
    p_new_handle->tdpl_io = tdpl_create(p_opt->io_thread_num, 51200);
    if (p_new_handle->tdpl_io == NULL) {
        log_err("ACM:Failed to create thread-pool for io!%s", strerror(errno));
        goto err2_ret;
    }

    log_info("ACM:Creating worker thread pool.");
    p_new_handle->tdpl_worker = tdpl_create(p_opt->worker_thread_num, 51200);
    if (p_new_handle->tdpl_worker == NULL) {
        log_err("ACM:Failed to create thread-pool for worker!%s", strerror(errno));
        goto err2_ret;
    }

    log_info("ACM:Creating channel memory pool.");
    struct mmpl_opt mmpl_opt;
    mmpl_opt.boundary = MMPL_BOUNDARY_2K;  // 2K对齐
    mmpl_opt.max_free_index = 51200;  // 最大空闲
    p_new_handle->mmpl = mmpl_create(&mmpl_opt);
    if (p_new_handle->mmpl == NULL) {
        log_err("ACM:Failed to create memory-pool!%s", strerror(errno));
        goto err2_ret;
    }


    p_new_handle->request_map = mmpl_getmem(p_new_handle->mmpl, p_opt->max_hold_req_num * sizeof(struct acm_request_map_entry));
    if (p_new_handle->request_map == NULL) {
        log_err("ACM:Failed to get memeory from mmpl for request_map!");
        goto err2_ret;
    }
    p_new_handle->request_id = 0;

    log_info("ACM:Creating epoll.");
    if ((p_new_handle->epoll_fd = epoll_create(1)) == -1) {
        log_err("ACM:Failed to create epoll:%s", strerror(errno));
        goto err2_ret;
    }

    log_info("ACM:Creating event loop-thread.");
    if(pthread_create(&tid, NULL, acm_event_loop, p_new_handle) == -1){ 
        log_err("ACM:Failed to create thread for event-loop.");
        goto err2_ret;
    }
    
    return p_new_handle;

err2_ret:
    free(p_new_handle);
err1_ret:
    return NULL;
}


/* 函数名: struct acm_channel* acm_connect(struct acm_handle p_handle, const char* ip, int port) 
 * 功能: 连接agent-server
 * 参数: const char* ip,
         int port,
 * 返回值: acm_channel
 */
struct acm_channel* acm_connect(
        struct acm_handle *p_acm_handle, 
        const char* ip, 
        int port){

    if (p_acm_handle == NULL) {
        goto err1_ret;
    }

    struct acm_channel *p_channel;
    struct epoll_event event;
    struct sockaddr_in servaddr;  
    int socket_fd;
    int no = 1;

    /*先进行套接字连接*/
    log_info("ACM:Creating socket for agent-client.");
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        log_err("ACM:Failed to create socket when connecting.");
    }
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;  
    servaddr.sin_port = htons((port));
    inet_pton(AF_INET, ip, &servaddr.sin_addr);
    log_info("ACM:Connecting to agent-server..");
    if (connect(socket_fd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in)) == -1) {
        log_err("ACM:Failed to connect server(ip:%s)!!%s", ip, strerror(errno));
        close(socket_fd);
        goto err1_ret;
    }

    /*接着进行channel初始化*/
    log_info("ACM:Connected successfully!Creating new channel...");
    p_channel = mmpl_getmem(p_acm_handle->mmpl, sizeof(struct acm_channel));
    p_channel->p_handle = p_acm_handle;
    p_channel->socket_fd = socket_fd;
    p_channel->write_queue = mmpl_getmem(p_acm_handle->mmpl, p_acm_handle->max_write_queue_len * sizeof(struct acm_write_task));
    if (p_channel->write_queue == NULL) {
        log_err("ACM:Failed to get memeory from mmpl for write_queue!");
        goto err2_ret;
    }

    p_channel->write_queue_head = 0;
    p_channel->write_queue_tail = 1;
    p_channel->is_head = 1;  // 从“头”开始
    p_channel->head_read_index = 0;
    p_channel->events = EPOLLIN | EPOLLRDHUP | EPOLLET;  // 初始化为读边缘出发
    pthread_spin_init(&p_channel->write_queue_spinlock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&p_channel->write_queue_consume_spinlock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&p_channel->reading_spinlock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init(&p_channel->writing_spinlock, PTHREAD_PROCESS_PRIVATE);
    p_channel->is_reding = 0;
    p_channel->is_writing = 0;
    p_channel->is_write_queue_empty = 0;
    p_channel->request_num = 0;
    //sem_init(&p_channel->write_queue_mutex, 0, 1);
    
    /*设置套接字*/
    if(setsockopt(p_channel->socket_fd, 
                IPPROTO_TCP, 
                TCP_NODELAY, 
                &no, 
                sizeof(no)) < 0){
        log_err("ACM:Failed to set agent_client_socket_fd!");
    }

    /*注册事件*/
    log_info("ACM:Add event to epoll for new channel.");
    event.data.ptr = p_channel;
    event.events = p_channel->events;
    if (epoll_ctl(p_acm_handle->epoll_fd,
                EPOLL_CTL_ADD,
                p_channel->socket_fd,
                &event) == -1) {
        log_err("ACM:Failed to ADD sockfd to epoll when connecting:%s",strerror(errno));
        goto err3_ret;
    }

    return p_channel;


err3_ret:
    mmpl_rlsmem(p_acm_handle->mmpl, p_channel->write_queue);
err2_ret:
    pthread_spin_destroy(&p_channel->write_queue_spinlock);
    //sem_destroy(&p_channel->write_queue_mutex);
    mmpl_rlsmem(p_acm_handle->mmpl, p_channel);
err1_ret:
    return NULL;
}


/* 函数名: int acm_epoll_mod(struct acm_channel *p_channel, unsigned int events) 
 * 功能: 修改epoll事件
 * 参数: struct acm_channel *p_channel,
         unsigned int events,
 * 返回值: 
 */
int acm_epoll_mod(struct acm_channel *p_channel, unsigned int events){
    struct epoll_event event;

    event.data.ptr = p_channel;
    event.events = events | EPOLLET;  // 边缘出发,只触发一次
    if (epoll_ctl(p_channel->p_handle->epoll_fd,
                EPOLL_CTL_MOD,
                p_channel->socket_fd,
                &event) == -1) {
        log_err("ACM:Failed to MOD sockfd to epoll for EPOLLOUT when requesting:%s",strerror(errno));
        return -1;
    }

    return 1;
}


/* 函数名: int acm_reqeust(struct acm_channel* p_channel, char* buf, int buf_size, void (*listener)(void* arg), void *arg) 
 * 功能: 发送数据
 * 参数: struct acm_channel* p_channel, 
         char* buf,
         int buf_size,
         void (*listener)(void *arg),
         void *arg,
 * 返回值: 
 */
int acm_request(
        struct acm_channel* p_channel, 
        char* buf, int buf_size, 
        void(*listening)(void *arg, char *buf, int data_size), 
        void *arg){

    if (p_channel == NULL) {
        return -1;
    }

    int request_id;
    int is_queue_empty = 0;
    struct acm_write_task *p_write_task;
    struct acm_handle *p_handle = p_channel->p_handle;

    /*由原子增操作获得唯一的request_id*/
    request_id = __sync_add_and_fetch(&p_handle->request_id, 1);
    /*hold住请求，等待响应*/
    log_debug("ACM:Hold reqeuest for id:%d", request_id);
    acm_hold_request(p_channel, request_id, listening, arg);  

    /*入write-task队列，跟io线程和其它执行到这里的线程抢锁*/
    /*不知道抢锁会不会很激烈，这里是需要考虑优化的地方*/
    //while(sem_trywait(&p_channel->write_queue_mutex) < 0);
    //sem_wait(&p_channel->write_queue_mutex);
    pthread_spin_lock(&p_channel->write_queue_spinlock);
    if (p_channel->write_queue_head + 1 == p_channel->write_queue_tail) {
        is_queue_empty = 1;
    }
    p_write_task = &p_channel->write_queue[(p_channel->write_queue_tail++)%p_handle->max_write_queue_len];
    p_write_task->data = buf;
    p_write_task->head.data_size = buf_size;
    p_write_task->head.req_id = request_id;
    p_write_task->write_index = 0;
    pthread_spin_unlock(&p_channel->write_queue_spinlock);
    //sem_post(&p_channel->write_queue_mutex);


    /* 如果本身队列是空的，则说明现在的epoll没有注册可写事件,并且没有线程对该
     * channel进行写操作，则向epoll注册可写事件，由epoll产生的可写事件触发
     * io-write线程来消费队列*/
    if (is_queue_empty == 1) {
        log_debug("ACM:Register EPOLLOUT to epoll when adding write-task to empty queue!");
        /*释放消费锁，说明队列非空，需要有线程来消费写队列*/
        pthread_spin_unlock(&p_channel->write_queue_consume_spinlock);
        //pthread_spin_lock(&p_channel->write_queue_empty_spinlock);
        //__sync_add_and_fetch(&p_channel->is_write_queue_empty, 1);
        // 添加可读(保证不出错)可写事件
        if (acm_epoll_mod(p_channel, EPOLLOUT | EPOLLIN | EPOLLRDHUP) < 0) {
            log_err("ACM:Failed to MOD sockfd to epoll for EPOLLOUT when requesting:%s",strerror(errno));
            return -2;
        }
    }

    return 1;
}

/* 函数名: void acm_io_read_thread(void *arg) 
 * 功能: IO读处理线程
 * 参数: void *arg, 指向channel
 * 返回值: 
 */
void acm_io_read_thread(void *arg){
    struct acm_channel *p_channel = arg;
    struct acm_handle *p_handle = p_channel->p_handle;
    int data_size;
    int read_size;

    /*接收头*/
    while (1) {
        if (p_channel->is_head == 1) {
            read_size = recv(p_channel->socket_fd, 
                    p_channel->head + p_channel->head_read_index, 
                    ACM_MSG_HEAD_SIZE - p_channel->head_read_index, 
                    MSG_DONTWAIT);

            if (read_size < 0) {
                if (errno == EAGAIN) {
                    break;
                }
                log_err("ACM:Some error occured when reading data from socket!%s", strerror(errno));
                break;
            }

            p_channel->head_read_index += read_size;

            if (p_channel->head_read_index < ACM_MSG_HEAD_SIZE) {
                /*如果头没有接收完整*/
                break;
            } else {
                data_size = acm_bytes2int(p_channel->head, 8);
                p_channel->is_head = 0;
                p_channel->recv_msg = mmpl_getmem(p_handle->mmpl, sizeof(struct acm_msg) + data_size);
                p_channel->recv_msg->data = (void *)p_channel->recv_msg + sizeof(struct acm_msg);
                p_channel->recv_msg->head.req_id = acm_bytes2long(p_channel->head, 0);
                p_channel->recv_msg->head.data_size = data_size;
                p_channel->recv_msg->p_channel = p_channel;
                p_channel->data_read_index = 0;
            }
        }

        log_debug("ACM:Recv head, req_id:%ld, data_size:%d", p_channel->recv_msg->head.req_id, p_channel->recv_msg->head.data_size);

        /*接收data*/
        read_size = recv(p_channel->socket_fd, 
                p_channel->recv_msg->data + p_channel->data_read_index, 
                p_channel->recv_msg->head.data_size - p_channel->data_read_index, 
                MSG_DONTWAIT);

        if (read_size < 0) {
            if (errno == EAGAIN) {
                break;
            }
            log_err("ACM:Some error occured when reading data from socket!%s", strerror(errno));
            break;
        }

        p_channel->data_read_index += read_size;

        if (p_channel->data_read_index < p_channel->recv_msg->head.data_size) {
            break;
        } else {
            /*表示一次报文接收完毕*/
            /*针对这次接收到的报文，执行工作线程*/
            __sync_sub_and_fetch(&p_channel->request_num, 1);  // 请求数减一
            if (tdpl_call_func(p_channel->p_handle->tdpl_worker, acm_response, p_channel->recv_msg) < 0) {
                log_err("Failed to start io thread for acm_io_do_write:%s", strerror(errno));
                mmpl_rlsmem(p_handle->mmpl, p_channel->recv_msg);
                break;
            }

            p_channel->is_head = 1;
            p_channel->head_read_index = 0;
        }
    }

    /*读处理完毕之后，原子操作告知处理完毕，并且注册可读写事件*/
    pthread_spin_unlock(&p_channel->reading_spinlock);
    //__sync_fetch_and_sub(&p_channel->is_reding, 1);
    if (acm_epoll_mod(p_channel, EPOLLOUT | EPOLLIN | EPOLLRDHUP) < 0) {
        log_err("ACM:Failed to MOD sockfd to epoll for EPOLLOUT when doing read:%s",strerror(errno));
    }
}

/* 函数名: void acm_io_write_thread(void *arg) 
 * 功能: IO写处理线程
 * 参数: void *arg, 指向channel
 * 返回值: 
 */
void acm_io_write_thread(void *arg){
    //log_debug("ACM:Stated io-write thread!");

    struct acm_channel *p_channel = arg;
    struct acm_handle *p_handle = p_channel->p_handle;
    struct acm_write_task *p_task;
    struct acm_write_task *write_task_queue = p_channel->write_queue;
    int hava_write_size;
    int is_queue_empty = 0;
    char head[ACM_MSG_HEAD_SIZE];

    /* 根据当前的结构设计，既然产生了可写事件，并且调用了写处理线程，则说明写队列
     * 一定是非空的！如果是空的，则说明某个地方出现了问题*/
    if (p_channel->write_queue_head + 1 == p_channel->write_queue_tail) {
        log_err("ACM:The write-queue is empty when calling io-write-thread!");
        return;
    }

    /*一直进行写操作，直到写队列为空，或者TCP写缓存为满*/
    while (is_queue_empty != 1) {
        /*从队头取出写任务*/
        p_task = &write_task_queue[(p_channel->write_queue_head + 1)%p_handle->max_write_queue_len];

        /*写头*/
        if (p_task->write_index < ACM_MSG_HEAD_SIZE) {
            log_debug("ACM:Writing head..req_id:%ld", p_task->head.req_id);
            acm_long2bytes(p_task->head.req_id, head, 0);
            acm_int2bytes(p_task->head.data_size, head, 8);
            if ((hava_write_size = send(p_channel->socket_fd,
                            head + p_task->write_index,
                            ACM_MSG_HEAD_SIZE - p_task->write_index,
                            MSG_DONTWAIT)) == -1) {
                if (errno != EAGAIN) {
                    log_err("ACM:Some error occured when write data to socket!%s", strerror(errno));
                    goto err_ret;
                }
                break;
            }
            log_debug("ACM:Have write head size:%d", hava_write_size);
            p_task->write_index += hava_write_size;
            if (ACM_MSG_HEAD_SIZE - p_task->write_index > 0) {
                /*如果没有读取完整，则注册可写事件，并且退出处理线程*/
                break;
            }
            log_debug("ACM:Writing data, req_id:%ld, data_size:%d",p_task->head.req_id,  p_task->head.data_size);
        }

        /*写data*/
        if ((hava_write_size = send(p_channel->socket_fd,
                        p_task->data + p_task->write_index - ACM_MSG_HEAD_SIZE,
                        p_task->head.data_size - p_task->write_index + ACM_MSG_HEAD_SIZE,
                        MSG_DONTWAIT)) == -1) {
            if (errno != EAGAIN) {
                log_err("ACM:Some error occured when write data to socket!%s", strerror(errno));
                goto err_ret;
            }
        }
        p_task->write_index += hava_write_size;
        log_debug("ACM:hava write size:%d for req_id:%ld", hava_write_size, p_task->head.req_id);
        if (p_task->head.data_size - (p_task->write_index - ACM_MSG_HEAD_SIZE)  > 0) {
            //log_debug("ACM:Write again!");
            /*如果没有写完整，则注册可写事件，并且退出处理线程*/
            break;
        }

        /*写完毕，把写队列的头节点去掉，并判断队列是否为空*/
        pthread_spin_lock(&p_channel->write_queue_spinlock);
        //while(sem_trywait(&p_channel->write_queue_mutex) < 0);
        p_channel->write_queue_head++;
        if (p_channel->write_queue_head + 1 == p_channel->write_queue_tail) {
            is_queue_empty = 1;
            /*队列已空，并且此次写过程完毕,不释放消费锁，因为空队列不允许被消费*/
            //pthread_spin_unlock(&p_channel->writing_spinlock);
            //__sync_fetch_and_sub(&p_channel->is_writing, 1);
            //__sync_fetch_and_sub(&p_channel->is_write_queue_empty, 1);
        }
        //sem_post(&p_channel->write_queue_mutex);
        pthread_spin_unlock(&p_channel->write_queue_spinlock);
    }

    if (is_queue_empty != 1) {
        /*非处理完毕的退出,写队列未消费为空，释放消费锁,允许有线程拿锁来消费队列*/
        //__sync_fetch_and_sub(&p_channel->is_writing, 1);
        //pthread_spin_unlock(&p_channel->writing_spinlock);
        pthread_spin_unlock(&p_channel->write_queue_consume_spinlock);
        if (acm_epoll_mod(p_channel, EPOLLOUT | EPOLLIN | EPOLLRDHUP) < 0) {
            log_err("ACM:Failed to MOD sockfd to epoll for EPOLLOUT when doing write:%s",strerror(errno));
        }
    }

    return;

err_ret:
    /*不释放消费锁，因为出错了，写队列就没必要去消费了*/
    //__sync_fetch_and_sub(&p_channel->is_writing, 1);
    //pthread_spin_unlock(&p_channel->writing_spinlock);
    return;
}


/* 函数名: int acm_io_do_read(struct acm_channel *p_channel) 
 * 功能: 负责从线程池里取出线程来处理io
 * 参数: struct hs_channel *p_channel,
 * 返回值: 
 */
int acm_io_do_read(struct acm_channel *p_channel){
    if (p_channel == NULL) {
        return -1;
    }

    if (tdpl_call_func(p_channel->p_handle->tdpl_io, acm_io_read_thread, p_channel) < 0) {
        log_err("Failed to start io thread for acm_io_do_read:%s", strerror(errno));
        return -2;
    }
    
    return 1;
}


/* 函数名: int acm_io_do_write(struct hs_channel *p_channel) 
 * 功能: 负责从线程池里取出线程来处理io
 * 参数: struct hs_channel *p_channel,
 * 返回值: 
 */
int acm_io_do_write(struct acm_channel *p_channel){

    if (p_channel == NULL) {
        return -1;
    }

    if (tdpl_call_func(p_channel->p_handle->tdpl_io, acm_io_write_thread, p_channel) < 0) {
        log_err("Failed to start io thread for acm_io_do_write:%s", strerror(errno));
        return -2;
    }
    
    return 1;
}


/* 函数名: void acm_event_loop(struct acm_handle *p_handle) 
 * 功能: epoll事件循环
 * 参数: struct acm_handle *p_handle,
 * 返回值: 
 */
void* acm_event_loop(void *arg){
    log_info("ACM:Started event-loop thread successfully!");

    struct acm_handle *p_handle = arg;

    struct epoll_event events[ACM_MAX_EPOLL_EVENTS];
    struct acm_channel *p_channel;
    int epoll_fd = p_handle->epoll_fd;
    int ready_num, i;

    while(1) {
        /*获取事件,若无事件，则永久阻塞*/
        ready_num = epoll_wait(epoll_fd, events, ACM_MAX_EPOLL_EVENTS, -1);
        if (ready_num == -1) {
            /*等待事件出现了错误*/
            log_err("ACM:Some error occured when waiting epoll events!%s", strerror(errno));
            sleep(1);
            continue;
        }

        for(i = 0;i < ready_num;i++) {
            if (events[i].events & EPOLLERR) {
                /*错误事件暂时不处理(在生产环境里一定要处理)*/
                log_err("ACM:EPOLLERR occured!");
                continue;
            }

            if (events[i].events & EPOLLRDHUP) {
                /*远端关闭事件,暂不多作处理，仅仅移出epoll*/
                log_err("ACM:EPOLLRDHUP occured!");
                p_channel = events[i].data.ptr;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, p_channel->socket_fd, NULL);
                continue;
            }

            if (events[i].events & EPOLLIN) {
                /*如果是可读事件*/
                p_channel = events[i].data.ptr;

                /*首先注销读事件*/
                //acm_epoll_mod(p_channel, events[i].events &= ~EPOLLIN);

                //if (__sync_fetch_and_add(&p_channel->is_reding, 1) == 0) {
                //    acm_io_do_read(p_channel);
                //} else {
                //    __sync_fetch_and_sub(&p_channel->is_reding, 1);
                //}

                if (pthread_spin_trylock(&p_channel->reading_spinlock) == 0) {
                    acm_io_do_read(p_channel);
                }
            }

            if (events[i].events & EPOLLOUT) {
                /*如果是可写事件*/
                p_channel = events[i].data.ptr;

                /*首先注销可写事件*/
                //acm_epoll_mod(p_channel, events[i].events &= ~EPOLLOUT);

                //if (__sync_fetch_and_add(&p_channel->is_write_queue_empty, 1) == 0) {
                //    /*队列是空的，则肯定不能执行写线程*/
                //    __sync_fetch_and_sub(&p_channel->is_write_queue_empty, 1);
                //    continue;
                //}

                //if (__sync_fetch_and_add(&p_channel->is_writing, 1) == 0) {
                //    /*保证只能运行一个写操作过程*/
                //    acm_io_do_write(p_channel);
                //} else {
                //    __sync_fetch_and_sub(&p_channel->is_writing, 1);
                //}

                if (pthread_spin_trylock(&p_channel->write_queue_consume_spinlock) == 0) {
                    /*同时只能有一个线程消费写队列*/
                    acm_io_do_write(p_channel);
                }
            }
        }
    }
    
}

/* 函数名: int acm_hold_request(struct acm_channel *p_channel, unsigned long req_id, void (*listening)(void* arg), void* arg) 
 * 功能: hold住request,注册listening
 * 参数: struct acm_channel *p_channel,
         unsigned long req_id,
         void (*listening)(void* arg),
         void* arg,
 * 返回值: 
 */
int acm_hold_request(struct acm_channel *p_channel, 
        unsigned long req_id, 
        void (*listening)(void* arg, char *resp_data, int data_size), 
        void* arg){

    if (p_channel == NULL) {
        return -1;
    }

    struct acm_handle *p_handle = p_channel->p_handle;
    struct acm_request_map_entry *p_entry;

    p_entry = &p_handle->request_map[req_id%p_handle->max_hold_req_num];
    log_debug("ACM:Get a request-map entry.");
    if (p_entry->req_id != 0) {
        /*如果map的entry并不是空的，说明map长度不够长，map长度应大于预估的最大请求量*/
        /*如果想要灵活的hold request，则应该使用红黑树*/
        log_err("ACM:The request map is full now!!!!");
        return -2;
    }
    p_entry->req_id = req_id;
    p_entry->listening = listening;
    p_entry->arg = arg;

    return 1;
}

/* 函数名: int acm_response(unsigned long req_id, char *resp_data) 
 * 功能: 响应，从request_map里取出注册的listening，并执行listening函数
 * 参数: void *arg, 指向acm_msg
 * 返回值: 
 */
void acm_response(void *arg){
    struct acm_msg *p_msg = arg;
    struct acm_channel *p_channel = p_msg->p_channel;
    struct acm_handle *p_handle = p_channel->p_handle;
    struct acm_request_map_entry *p_entry;
    unsigned long req_id = p_msg->head.req_id;

    p_entry = &p_handle->request_map[req_id%p_handle->max_hold_req_num];
    p_entry->req_id = 0;
    if (p_entry->listening == NULL) {
        log_warning("ACM:listening is not setted for req_id:%ld!", req_id);
        return;
    }
    p_entry->listening(p_entry->arg, p_msg->data, p_msg->head.data_size);
    p_entry->listening = NULL;

    /*执行完毕之后，应该释放msg*/
    mmpl_rlsmem(p_handle->mmpl, p_msg);

    return;
}

void acm_int2bytes(unsigned int value, char* buf, int offset){
    buf[offset + 3] = (unsigned char)value;
    buf[offset + 2] = (unsigned char)(value >> 8);
    buf[offset + 1] = (unsigned char)(value >> 16);
    buf[offset + 0] = (unsigned char)(value >> 24);
}

unsigned int acm_bytes2int(char* buf, int offset){
    return ((unsigned int)buf[offset + 3] & 0xFF) |
        (((unsigned int)buf[offset + 2] & 0xFF) << 8) |
        (((unsigned int)buf[offset + 1] & 0xFF) << 16) |
        (((unsigned int)buf[offset + 0] & 0xFF) << 24);
}

void acm_long2bytes(unsigned long value, char* buf, int offset){
    buf[offset + 7] = (unsigned char)value;
    buf[offset + 6] = (unsigned char)(value >> 8);
    buf[offset + 5] = (unsigned char)(value >> 16);
    buf[offset + 4] = (unsigned char)(value >> 24);
    buf[offset + 3] = (unsigned char)(value >> 32);
    buf[offset + 2] = (unsigned char)(value >> 40);
    buf[offset + 1] = (unsigned char)(value >> 48);
    buf[offset + 0] = (unsigned char)(value >> 56);
}

unsigned long acm_bytes2long(char* buf, int offset) {
    return (((unsigned long)buf[offset + 7]) & 0xFFL) |
        (((unsigned long)buf[offset + 6] & 0xFFL) << 8) |
        (((unsigned long)buf[offset + 5] & 0xFFL) << 16) |
        (((unsigned long)buf[offset + 4] & 0xFFL) << 24) |
        (((unsigned long)buf[offset + 3] & 0xFFL) << 32) |
        (((unsigned long)buf[offset + 2] & 0xFFL) << 40) |
        (((unsigned long)buf[offset + 1] & 0xFFL) << 48) |
        (((unsigned long)buf[offset + 0] & 0xFFL) << 56);
           
}

