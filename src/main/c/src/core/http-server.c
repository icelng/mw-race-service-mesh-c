#include "http-server.h"
#include "stdio.h"
#include "string.h"
#include "log.h"
#include "sys/socket.h"
#include "sys/epoll.h"
#include "unistd.h"
#include "netinet/in.h"
#include "errno.h"
#include "ctype.h"

void hs_accept_thread(void *arg);
int hs_bind(int port);
void hs_io_thread(void *arg);
int hs_call_channel(struct hs_channel *p_hs_channel);
int hs_attempt_recall_channel(struct hs_channel *p_channel);
int hs_io_do_read(struct hs_channel *p_channel);
void hs_tolower(char *str);
int hs_io_do_write(struct hs_channel *p_channel);

/* 函数名: struct hs_handle* hs_start(struct hs_bootstrap *hs_bt) 
 * 功能: 启动http服务器
 * 参数: struct hs_bootstrap *hs_bt, 启动配置
 * 返回值: 服务器句柄
 */
struct hs_handle* hs_start(struct hs_bootstrap *hs_bt){
    struct hs_handle *p_new_hs_handle = NULL;
    int i;

    p_new_hs_handle = malloc(sizeof(struct hs_handle));
    if (p_new_hs_handle == NULL) {
        goto err1_ret;
    }
    p_new_hs_handle->max_connection = hs_bt->max_connection;
    p_new_hs_handle->buffer_size = hs_bt->buffer_size;
    p_new_hs_handle->content_handler = hs_bt->content_handler;

    /*创建线程池内存池*/
    log_info("Creating worker thread pool.");
    p_new_hs_handle->tdpl_worker = tdpl_create(hs_bt->worker_thread_num, 51200);  // n个线程，51200个等待
    if (p_new_hs_handle->tdpl_worker == NULL) {
        log_err("启动http-server时，创建线程池失败!%s", strerror(errno));
        goto err2_ret;
    }

    log_info("Creating io thread pool.");
    p_new_hs_handle->tdpl_io = tdpl_create(hs_bt->io_thread_num + 3, 128);  // 3个IO线程，128个等待
    if (p_new_hs_handle->tdpl_io == NULL) {
        log_err("启动http-server时，创建线程池失败!%s", strerror(errno));
        goto err2_ret;
    }

    log_info("Creating close thread pool.");
    /*这个方式会增加上下文切换的*/
    if (p_new_hs_handle->tdpl_io == NULL) {
        log_err("启动http-server时，创建线程池失败!%s", strerror(errno));
        goto err2_ret;
    }

    log_info("Creating channel memory pool.");
    struct mmpl_opt mmpl_opt;
    mmpl_opt.boundary = MMPL_BOUNDARY_1K;  // 1K对齐
    mmpl_opt.max_free_index = 5120;  // 最大空闲
    p_new_hs_handle->mmpl = mmpl_create(&mmpl_opt);
    if (p_new_hs_handle->mmpl == NULL) {
        log_err("启动http-server时，创建内存池失败!%s", strerror(errno));
        goto err2_ret;
    }

    /*创建服务端套接字*/
    log_info("Creating server socket and bind on %d.", hs_bt->server_port);
    p_new_hs_handle->sockfd = hs_bind(hs_bt->server_port);
    if (p_new_hs_handle->sockfd < 0){
        log_err("创建服务端套接字失败!ret_no:%d", p_new_hs_handle->sockfd);
        goto err2_ret;
    }

    /*创建epoll*/
    log_info("Creating epoll.");
    if((p_new_hs_handle->epoll_fd = epoll_create(1)) == -1){
        syslog(LOG_DEBUG,"启动http-server时，创建epoll失败:%s",strerror(errno));
        goto err2_ret;
    }

    /*启动io线程*/
    log_info("Starting io thread.");
    if (tdpl_call_func(p_new_hs_handle->tdpl_io, hs_io_thread, &p_new_hs_handle, sizeof(p_new_hs_handle)) < 0) {
        log_err("启动http-server时，启动io-read线程失败:%s", strerror(errno));
        goto err2_ret;
    }

    /*启动accept线程*/
    log_info("Starting accept thread.");
    if (tdpl_call_func(p_new_hs_handle->tdpl_io, hs_accept_thread, &p_new_hs_handle, sizeof(p_new_hs_handle)) < 0) {
        log_err("启动http-server时，启动accept线程失败:%s", strerror(errno));
        goto err2_ret;
    }


    return p_new_hs_handle;

err2_ret:
    free(p_new_hs_handle);
err1_ret:
    return NULL;
}

/* 函数名: int hs_new_connection(struct hs_handle* p_hs_h, int client_sockfd) 
 * 功能: 新建链接，进行各种初始化
 * 参数: int client_sockfd, 客户端套接字
 * 返回值: 
 */
int hs_new_connection(struct hs_handle* p_hs_handle, int client_sockfd){
    log_debug("New connectiong");
    int ret_value;

    /*创建decode_handle*/
    struct hs_channel *p_channel;
    p_channel = mmpl_getmem(p_hs_handle->mmpl, sizeof(struct hs_channel) + p_hs_handle->buffer_size);
    if (p_channel == NULL) {
        ret_value = -1;
        goto err1_ret;
    }
    /*初始化channel*/
    p_channel->p_hs_handle = p_hs_handle;
    p_channel->socket = client_sockfd;
    p_channel->decode_index = 0;
    p_channel->is_line = 1;
    p_channel->is_head = 1;
    p_channel->is_body = 1;
    p_channel->is_key = 1;
    p_channel->is_content_length = 0;
    p_channel->body_size = 0;
    p_channel->key_start = -1;
    p_channel->value_start = -1;
    p_channel->processing_index = -1;  // 将要处理到的字节
    p_channel->write_index = 0;
    p_channel->read_index = 0;
    p_channel->processing_index_now = -1;  // 当前将要处理到的字节
    p_channel->is_processing = 0;
    p_channel->buffer_size = p_hs_handle->buffer_size;
    p_channel->buffer = (char *)p_channel + sizeof(struct hs_channel);
    sem_init(&(p_channel->processing_mutex), 0, 1);

    /*向epoll注册读事件*/
    struct epoll_event event;
    event.data.ptr = p_channel;  
    event.events = EPOLLIN | EPOLLRDHUP;  //注册读事件，远端关闭事件，水平触发模式
    if(epoll_ctl(p_hs_handle->epoll_fd,
                EPOLL_CTL_ADD,
                client_sockfd,
                &event) == -1){  //把客户端的socket加入epoll
        syslog(LOG_DEBUG,"Failed to add sockfd to epoll for EPOLLIN:%s",strerror(errno));
        ret_value = -2;
        goto err2_ret;
    }

    return 1;

err2_ret:
    mmpl_rlsmem(p_hs_handle->mmpl, p_channel);
err1_ret:
    return ret_value;
    
}


/* 函数名: void hs_accept_thread(void *arg) 
 * 功能: 接受线程，负责接受链接请求
 * 参数: void *arg, hs_handle指针的指针
 * 返回值: 无
 */
void hs_accept_thread(void *arg){
    log_info("Start accept thread sucessfully!");

    struct sockaddr_in client_addr;
    struct hs_handle *hs_h = *(struct hs_handle**)arg;
    int sin_size;
    int client_sockfd;

    listen(hs_h->sockfd, hs_h->max_connection);
    log_info("Listening...max_connection:%d", hs_h->max_connection);

    while(1) {
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        sin_size = sizeof(struct sockaddr_in);
        //接受客户端的连接请求
        if((client_sockfd = accept(hs_h->sockfd, 
                        (struct sockaddr*)&client_addr, 
                        (socklen_t *)&sin_size)) < 0){
            syslog(LOG_DEBUG,"Accept error:%s",strerror(errno));
            continue;
        }
        /*建立新链接*/
        if (hs_new_connection(hs_h, client_sockfd) < 0) {
            log_err("Failed to create connection!%s", strerror(errno));
        }
        log_debug("Create new connection successfully!");
    }
}


/* 函数名: void hs_io_read_thread(void *arg) 
 * 功能: io读线程，负责读写tcp缓存并调用channel，使用epoll监听读写事件
 * 参数: void *arg, hs_handle指针的指针
 * 返回值: 
 */
void hs_io_thread(void *arg){
    log_info("Start io thread successfully!");

    struct hs_handle *p_hs_handle = *(struct hs_handle**)arg;
    struct epoll_event events[MAX_EPOLL_EVENTS];
    struct hs_channel *p_channel;
    int epoll_fd = p_hs_handle->epoll_fd;
    int ready_num, i;

    while(1) {
        /*获取事件,若无事件，则永久阻塞*/
        ready_num = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        if (ready_num == -1) {
            /*等待事件出现了错误*/
            log_err("Some error occured when waiting epoll events!%s", strerror(errno));
            sleep(1);
            continue;
        }

        for(i = 0;i < ready_num;i++) {
            if (events[i].events & EPOLLERR) {
                /*错误事件暂时不处理(在生产环境里一定要处理)*/
                log_err("EPOLLERR occured!");
                continue;
            }

            if (events[i].events & EPOLLRDHUP) {
                /*远端关闭事件,暂不多作处理，仅仅移出epoll*/
                log_err("EPOLLRDHUP occured!");
                p_channel = events[i].data.ptr;
                epoll_ctl(p_channel->p_hs_handle->epoll_fd, EPOLL_CTL_DEL, p_channel->socket, NULL);
                continue;
            }

            if (events[i].events & EPOLLIN) {
                /*如果是可读事件*/
                p_channel = events[i].data.ptr;
                hs_io_do_read(p_channel);
            }

            if (events[i].events & EPOLLOUT) {
                /*如果是写事件*/
                //log_err("EPOLLOUT occured!");
                p_channel = events[i].data.ptr;
                hs_io_do_write(p_channel);

            }
        }
    }
}


/* 函数名: int hs_io_do_write(struct hs_channel *p_channel) 
 * 功能: 把channel缓存数据写到tcp发送缓存里
 * 参数: struct hs_channel *p_channel,
 * 返回值: 
 */
int hs_io_do_write(struct hs_channel *p_channel){
    int hava_write_size;
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;

    if (p_channel->write_index == p_channel->write_size) {
        /*写完毕,把套接字移出epoll,关闭链接，释放channel*/
        epoll_ctl(p_hs_handle->epoll_fd, EPOLL_CTL_DEL, p_channel->socket, NULL);
        close(p_channel->socket);
        mmpl_rlsmem(p_hs_handle->mmpl, p_channel);
        return 1;
    }

    if ((hava_write_size = send(p_channel->socket,
                    p_channel->buffer + p_channel->write_index,
                    p_channel->write_size - p_channel->write_index,
                    MSG_DONTWAIT)) == -1) {
        log_err("Some error occured when write data to socket!%s", strerror(errno));
        return -1;
    }
    p_channel->write_index += hava_write_size;
    
    return 1;
}

/* 函数名: int hs_io_do_read(struct hs_channel *p_channel) 
 * 功能: 把tcp缓存的数据读取到channel里,并且使用一个Worker线程调用channel
 * 参数: struct hs_channel *p_channel,
 * 返回值: 
 */
int hs_io_do_read(struct hs_channel *p_channel){
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;
    int read_size;

    /*开始把TCP读缓存上的数据拷贝到自己指定的buffer里*/
    read_size = recv(p_channel->socket, 
            p_channel->buffer + p_channel->read_index, 
            p_hs_handle->buffer_size - p_channel->read_index, 
            MSG_DONTWAIT);
    if (read_size <= 0) {
        /*暂时不处理*/
        log_err("Some error occured when reading data from socket!%s", strerror(errno));
        return -1;
    }
    p_channel->read_index += read_size;
    if (p_channel->read_index >= p_channel->buffer_size) {
        log_err("Will no enought space to read bytes!!");
        /*从epoll中移除,将不再读取数据*/
        epoll_ctl(p_hs_handle->epoll_fd, EPOLL_CTL_DEL, p_channel->socket, NULL);
    }
    /*调用channel进行处理*/
    hs_call_channel(p_channel);
    
    return read_size;
}


/* 函数名: void hs_decoder(void *arg) 
 * 功能: 解码器(好长好臭)，channel责任链中第一个节点
 * 参数: void *arg, channel的指针
 * 返回值: 
 */
void hs_decoder(void *arg){
    struct hs_channel *p_channel = *((struct hs_channel**)arg);
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;
    char *buffer = p_channel->buffer;
    char parse_char;

    //log_debug("Start channel!processing_index_now:%d", p_channel->processing_index_now);
    //log_debug("%s", buffer);
    /*遍历buffer，解码*/
    for(;p_channel->decode_index <= p_channel->processing_index_now;p_channel->decode_index++) {
        parse_char = buffer[p_channel->decode_index];

        if (p_channel->is_line == 1) {
            /*请求行,现在不关注*/
            if (parse_char == 0x0D) {
                /*回车*/
                buffer[p_channel->decode_index] = 0;
                log_debug("Request line:%s", buffer);
                continue;
            }
            if (parse_char == 0x0A) {
                /*换行,下一行是头(head)*/
                p_channel->is_line = 0;
                p_channel->key_start = p_channel->decode_index + 1;
            }
        } else if (p_channel->is_head == 1) {
            /*请求头，现在只关注Content-Length*/
            if (p_channel->is_key == 1) {
                /*如果解析的是key*/
                if (parse_char == ':') {
                    /*key解析结束*/
                    p_channel->is_key = 0;
                    buffer[p_channel->decode_index] = 0;  // 设置字符串结束标志
                    p_channel->value_start = p_channel->decode_index + 1;
                    hs_tolower(&buffer[p_channel->key_start]);  // 保证是小写
                    if (!strcmp(&buffer[p_channel->key_start], "content-length")) {
                        /*如果是content的大小,打上标志*/
                        p_channel->is_content_length = 1;
                    }
                    continue;
                }
                if (parse_char == 0x0D) {
                    /*回车*/
                    continue;
                }
                if (parse_char == 0x0A) {
                    /*换行，下一行是body部分*/
                    p_channel->is_head = 0;
                    p_channel->body_start = p_channel->decode_index + 1;
                    continue;
                }
            } else {
                /*如果解析的是value*/
                if (parse_char == 0x0D) {
                    /*回车,解析value结束*/
                    if (p_channel->is_content_length == 1) {
                        /*只关注Content-length*/
                        p_channel->is_content_length = 0;
                        buffer[p_channel->decode_index] = 0;  // 设置字符串结束标志
                        p_channel->body_size = atoi(&buffer[p_channel->value_start]);
                    }
                    continue;
                }
                if (parse_char == 0x0A) {
                    /*换行，解析下一个key-value*/
                    p_channel->is_key = 1;
                    p_channel->key_start = p_channel->decode_index + 1;
                    continue;
                }
            }
        } else if (p_channel->is_body == 1) {
            log_debug("Decode body!");
            /*请求体处理，确保请求体接受完整，直接跳出循环*/
            p_channel->decode_index = p_channel->processing_index_now;
            if (p_channel->processing_index_now - p_channel->body_start + 1 == p_channel->body_size) {
                /*表明body接受完毕*/
                p_channel->is_body = 0;
                /*调用content处理函数*/
                if (p_hs_handle->content_handler != NULL) {
                    buffer[p_channel->processing_index_now + 1] = 0;
                    p_hs_handle->content_handler(p_channel, p_channel->body_size, &buffer[p_channel->body_start]);
                } else {
                    log_warning("The content handler is not setted!");
                }
                /*直接返回，不然下来的代码会添加套接字到epoll*/
                return;
            }
        }
    }

    /*向epoll注册读事件，表示要继续读取数据*/
    struct epoll_event event;
    event.data.ptr = p_channel;  
    event.events = EPOLLIN | EPOLLRDHUP;  //注册读事件，远端关闭事件，水平触发模式
    if(epoll_ctl(p_hs_handle->epoll_fd,
                EPOLL_CTL_ADD,
                p_channel->socket,
                &event) == -1){  //把客户端的socket加入epoll
        syslog(LOG_DEBUG,"Failed to add sockfd to epoll for EPOLLIN:%s",strerror(errno));
    }

    /* 到此，将会结束一次chennel的调用，然后尝试重新调用channel */
    /* 查看将要处理的processing_index是否大于正在处理的processing_index_now
     * 如果大于，直接在当前发起处理请求，把处理请求发送到Worker线程池队列*/
    //hs_attempt_recall_channel(p_channel);
}

/* 函数名: int hs_response_ok(struct hs_channel *p_channel, char *response_body, int body_size) 
 * 功能: 发送响应，响应状态是200,类型是"text/html;charset=utf-8"
 * 参数: char *response_body, 相应body缓存
         int body_size, body大小
 * 返回值: 
 */
int hs_response_ok(struct hs_channel *p_channel, char *response_body, int body_size){
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;
    int write_index = 0;
    char head[32];  // 32字节的栈空间，不过分吧
    int head_length;
    char *buffer = p_channel->buffer;
    struct epoll_event event;

    /*组装响应行和头*/
    int response_line_length = strlen(HTTP_SERVER_RESPONSE_OK);
    memcpy(&buffer[write_index], HTTP_SERVER_RESPONSE_OK, response_line_length);
    write_index += response_line_length;

    head_length = strlen(HTTP_SERVER_CONTENT_TYPE);
    memcpy(&buffer[write_index], HTTP_SERVER_CONTENT_TYPE, head_length);
    write_index += head_length;

    sprintf(head, "content-length:%d\r\n\r\n", body_size);
    head_length = strlen(head);
    memcpy(&buffer[write_index], head, head_length);
    write_index += head_length;

    /*把响应body复制到channel的buffer里*/
    if (write_index + body_size >= p_channel->buffer_size) {
        log_err("The size(%d) of response is larger than buffer_size:%d", write_index + body_size, p_channel->buffer_size);
        return -1;
    }
    memcpy(&buffer[write_index], response_body, body_size);

    /*初始化IO线程所需要的相关变量*/
    p_channel->write_index = 0;
    p_channel->write_size = write_index + body_size;

    /*往epoll添加可写事件监听*/
    event.data.ptr = p_channel;
    event.events = EPOLLOUT;  // 设置可写事件,水平触发模式
    if (epoll_ctl(p_hs_handle->epoll_fd,
                EPOLL_CTL_ADD,
                p_channel->socket,
                &event) == -1) {
        syslog(LOG_DEBUG,"Failed to add sockfd to epoll for EPOLLOUT:%s",strerror(errno));
        return -2;
    }
    
    return 1;
}

/* 函数名: int hs_attempt_recall_channel(struct hs_channel *p_channel) 
 * 功能: 尝试重新调用decoder
 * 参数: struct hs_channel *p_channel,
 * 返回值: 
 */
int hs_attempt_recall_channel(struct hs_channel *p_channel){
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;

    /* 跟IO线程抢锁(其会调用hs_attempt_call_channel)，IO线程可能会为此阻塞，如果
     * 测试性能不好，会优化*/
    sem_wait(&p_channel->processing_mutex);
    if (p_channel->processing_index > p_channel->processing_index_now) {
        p_channel->processing_index_now = p_channel->processing_index;
        if (tdpl_call_func(p_hs_handle->tdpl_worker, hs_decoder, &p_channel, sizeof(p_channel)) < 0) {
            log_err("Failed to continue(restart) worker thread for channel:%s", strerror(errno));
            sem_post(&p_channel->processing_mutex);
            return -1;
        }
    } else {
        p_channel->is_processing = 0;
    }
    sem_post(&p_channel->processing_mutex);
    
    return 1;
}


/* 函数名: int hs_attempt_call_channel(struct hs_handle *p_hs_handle, struct hs_channel *p_hs_channel) 
 * 功能: 尝试调用decoder，如果channel已经配有线程正在处理，则放弃再次启动
 * 参数: struct hs_handle *p_hs_handle,
         struct hs_channel *p_hs_channel,
 * 返回值: 
 */
int hs_call_channel(struct hs_channel *p_channel){
    struct hs_handle *p_hs_handle = p_channel->p_hs_handle;

    p_channel->processing_index = p_channel->read_index - 1;
    p_channel->processing_index_now = p_channel->processing_index;
    if (tdpl_call_func(p_hs_handle->tdpl_worker, hs_decoder, &p_channel, sizeof(p_channel)) < 0) {
        log_err("Failed to start worker thread for channel:%s", strerror(errno));
        return -1;
    }
    epoll_ctl(p_hs_handle->epoll_fd, EPOLL_CTL_DEL, p_channel->socket, NULL);

    return 1;
}


/* 函数名: int hs_bind(int port) 
 * 功能: 创建socket并且绑定端口
 * 参数: int port, 端口
 * 返回值: 套接字描述符
 */
int hs_bind(int port){
    int no = 1;
    int server_sockfd;
    struct sockaddr_in server_addr;
    

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        return -1;
    }
    /*设置允许端口重用*/
    if(setsockopt(server_sockfd, 
                SOL_SOCKET, 
                SO_REUSEADDR, 
                &no, 
                sizeof(no)) < 0){
        return -2;
    }
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if(bind(server_sockfd, 
                (struct sockaddr*)&server_addr, 
                sizeof(struct sockaddr_in)) == -1){
        return -3;
    }

    return server_sockfd;
}

/* 函数名: void hs_tolower(char *str) 
 * 功能: 把字符串都转换成小写，注意，一定要字符串
 * 参数: char *str,
 * 返回值: 
 */
void hs_tolower(char *str){
    int i;
    int len = strlen(str);

    for (i = 0;i < len;i++) {
        str[i] = tolower(str[i]);
    }
}

/* 函数名: int hex2dec(char c) 
 * 功能: 
 * 参数: char c,
 * 返回值: 
 */
int hex2dec(char c){
    if ('0' <= c && c <= '9') {  
        return c - '0';  
    } else if ('a' <= c && c <= 'f') {  
        return c - 'a' + 10;  
    } else if ('A' <= c && c <= 'F') {  
        return c - 'A' + 10;  
    } else {  
        return -1;  
    }  
}


/* 函数名: void hs_url_decode(char *url) 
 * 功能: url解码
 * 参数: char *url,
 * 返回值: 
 */
void hs_url_decode(char *url){
    int i = 0;  
    int len = strlen(url);  
    int res_len = 0;  
    char res[HTTP_URL_DECODE_BUFSIZE];  // 使用栈空间作中间缓存，快，但不通用
    for (i = 0; i < len; ++i) {  
        char c = url[i];  
        if (c != '%') {  
            res[res_len++] = c;  
        } else {  
            char c1 = url[++i];  
            char c0 = url[++i];  
            int num = 0;  
            num = hex2dec(c1) * 16 + hex2dec(c0);  
            res[res_len++] = num;  
        }  
    }  
    res[res_len] = '\0';  
    strcpy(url, res);  
}

