#include "tdpool.h"  
#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "mmpool.h"


void *tdpl_worker_thread(void *arg);
void *tdpl_master_thread(void *arg);


/* 函数名: void tdpl_wktd_cleanup(void *arg)
 * 功能: worker线程清理函数
 * 参数: void *arg,指向线程信息结构体的指针
 * 返回值:
 */
void tdpl_wktd_cleanup(void *arg){
    struct tdpl_td_handle *p_td_handle;
    p_td_handle = (struct tdpl_td_handle*)arg;
    sem_destroy(&p_td_handle->run); //释放信号量
    
}

/* 函数名: void tdpl_mastertd_cleanup(void *arg)
 * 功能: master线程清理函数
 * 参数:
 * 返回值:
 */
void tdpl_mastertd_cleanup(void *arg){
    
}


/* 函数名: void *tdpl_destroy_thread(void *arg)
 * 功能: 线程池销毁线程
 * 参数: void *arg,指向线程池结构体的指针
 * 返回值:
 */
//void *tdpl_destroy_thread(void *arg){
//    struct tdpl_s *pts;
//    struct tdpl_td_i *p_tti;
//    int i;
//    pts = (struct tdpl_s *)arg;
//    /*取消所有的worker线程*/
//    for(i = 0;i < pts->thread_num;i++){
//        p_tti = &pts->tti_array[i];
//        pthread_cancel(p_tti->tid); //给worker线程发送取消信号
//        /*因为worker线程的取消点设置在sem_wait()的后面，所以得执行下面的语句*/
//        sem_post(&p_tti->run);
//        pthread_join(p_tti->tid,NULL); //等待线程取消完毕
//    }
//    /*销毁各种信号量*/
//    sem_destroy(&pts->req_list_mutex);
//    sem_destroy(&pts->avali_list_mutex);
//    sem_destroy(&pts->req_n);
//    sem_destroy(&pts->avali_td_n);
//    sem_destroy(&pts->req_n_emty);
//    /*销毁内存池*/
//    mmpl_destroy(pts->mmpl);
//}

/* 函数名: int tdpl_destroy(struct tdpl_s *pts)
 * 功能: 销毁线程池
 * 参数: struct tdpl_s *pts,指向线程池结构体的指针
 * 返回值: -1,
 *          1,
 */
//int tdpl_destroy(struct tdpl_s *pts){
//    unsigned long tid;
//
//    if(pts == NULL){
//        return -1;
//    }
//    pthread_cancel(pts->master_tid); //给master线程发送线程取消信号
//    /*因为master线程取消点是设置在等待两个信号量之后的位置，所以得进行两个V操作*/
//    sem_post(&pts->avali_td_n);
//    sem_post(&pts->req_n);
//    pthread_join(pts->master_tid,NULL); //等待master线程取消完毕
//    /*下面是消耗完请求队列的位置，即是拒绝指定函数的调用请求*/
//    while(sem_trywait(&pts->req_n_emty) != -1); 
//    /*启动线程池销毁线程，下来的清理工作交由线程tdpl_destroy_thread负责*/
//    if(pthread_create(&tid,NULL,tdpl_destroy_thread,pts) == -1){ 
//        return -1;
//    }
//    return 1;
//}

/* 函数名: void *tdpl_worker_thread(*arg)
 * 功能: 调用函数的线程
 * 参数: void *arg,指向该线程信息结构体的指针
 * 返回值:
 */
void *tdpl_worker_thread(void *arg){
    struct tdpl_call_node *p_call_node;
    struct tdpl_td_handle *p_td_handle; // 线程handle
    struct tdpl_s *pts;  //指向线程所属线程池的结构体
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); //设置线程可被取消
    p_td_handle = (struct tdpl_td_handle *)arg;
    pts = p_td_handle->p_tdpl_s;
    pthread_cleanup_push(tdpl_wktd_cleanup,p_td_handle);

    /*建立线程内存池(测试)*/
    struct mmpl_opt mmpl_opt;
    mmpl_opt.boundary = MMPL_BOUNDARY_1K;  // 1K对齐
    mmpl_opt.max_free_index = 51200;  // 最大空闲
    mmpl thread_mmpl = mmpl_create(&mmpl_opt);


    sem_post(&pts->ready_n);  // 表示工作线程已准备就绪

    while(1){
        sem_wait(&pts->call_wait_n); // 等待有调用请求

        /*请求队列出队，需抢读者锁*/
        //pthread_spin_lock(&pts->call_queue_read_spinlock);
        p_call_node = &pts->call_queue[__sync_add_and_fetch(&pts->call_queue_head, 1) % pts->call_queue_period];
        //pthread_spin_unlock(&pts->call_queue_read_spinlock);
        p_td_handle->call_func = p_call_node->call_func;
        p_td_handle->arg = p_call_node->arg;

        pthread_testcancel(); //设置线程取消点,在销毁线程池时，线程会被取消
        /*下一条语句为调用函数*/
        (p_td_handle->call_func)(p_td_handle->arg, thread_mmpl); 
        /* 函数执行完了之后，把该线程的信息结构体插入可用队列中,并且把参数所占用
         * 的内存空间还给内存池*/
        /* 有时候在调用函数的过程中接收到了线程取消信号,所以在函数执行完了之后马
         * 上取消线程。*/
        pthread_testcancel(); 
        p_td_handle->arg = NULL;
    }
    pthread_cleanup_pop(0);
}

/* 函数名: void *tdpl_master_thread(void *arg)
 * 功能: 管理worker线程的线程，通知worker执行
 * 参数: void *arg,指向线程池结构体的指针
 * 返回值:
 */
//void *tdpl_master_thread(void *arg){
//    struct tdpl_s *pts;
//    struct tdpl_td_handle *p_td_handle;  //线程信息结构体
//    struct tdpl_call_node *p_call_node;
//    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); //设置线程可被取消
//    pts = (struct tdpl_s*)arg;
//    pthread_cleanup_push(tdpl_mastertd_cleanup,NULL);
//    while(1){
//        sem_wait(&pts->avali_td_n); // 等待至有可用线程
//        sem_wait(&pts->call_wait_n);  // 等待有函数调用请求
//        pthread_testcancel();  // 设置线程取消点，销毁线程时，该线程会在这被取消
//        /*从可用线程队列中取出一个线程handle*/
//        p_td_handle = pts->avali_queue[(++pts->avali_queue_head) % pts->avali_queue_period];
//        /*从请求队列中取出一个请求,读者锁为自旋锁*/
//        while(sem_trywait(&pts->call_queue_read_mutex) < 0);
//        p_call_node = &pts->call_queue[(++pts->call_queue_head) % pts->call_queue_period];
//        sem_post(&pts->call_queue_read_mutex);
//        p_td_handle->call_func = p_call_node->call_func;
//        p_td_handle->arg = p_call_node->arg;
//        sem_post(&p_td_handle->run); // 告知worker线程开始调用函数
//    }
//    pthread_cleanup_pop(0);
//}




/* 函数名: struct tdpl_s* tdpl_create(unsigned int thread_num,unsigned int max_wait_n)
 * 功能: 创建线程池
 * 参数: int thread_num,线程池的线程数
 *       int max_wait_n,最大请求调用线程的等待数
 * 返回值: NULL,创建出错
 *        !NULL,线程池结构体指针
 */
struct tdpl_s* tdpl_create(int thread_num,int max_wait_n){
    struct tdpl_s *p_new_tdpl_s;
    struct tdpl_td_handle *p_td_handle; //指向线程池的worker线程handle
    int i;

    /*创建线程池结构体*/
    p_new_tdpl_s = malloc(sizeof(struct tdpl_s));
    if(p_new_tdpl_s == NULL){
        goto err1_ret;
    }
    /*为worker线程信息结构体数组安排内存空间*/
    p_new_tdpl_s->td_handle_array = malloc(thread_num * sizeof(struct tdpl_td_handle));
    if(p_new_tdpl_s->td_handle_array == NULL){
        goto err2_ret;
    }
    p_new_tdpl_s->thread_num = thread_num;
    p_new_tdpl_s->max_wait_n = max_wait_n;

    /*初始化各种信号量*/
    sem_init(&p_new_tdpl_s->ready_n, 0, 0 - thread_num + 1);
    //sem_init(&p_new_tdpl_s->avali_td_n, 0, thread_num);
    sem_init(&p_new_tdpl_s->call_wait_n, 0, 0);
    //sem_init(&p_new_tdpl_s->call_queue_write_mutex, 0, 1);
    //sem_init(&p_new_tdpl_s->avali_queue_write_mutex, 0, 1);
    //sem_init(&p_new_tdpl_s->call_queue_read_mutex, 0, 1);
    //pthread_spin_init(&p_new_tdpl_s->call_queue_read_spinlock, PTHREAD_PROCESS_PRIVATE);
    //pthread_spin_init(&p_new_tdpl_s->call_queue_write_spinlock, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&p_new_tdpl_s->call_queue_read_lock, NULL);
    pthread_mutex_init(&p_new_tdpl_s->call_queue_write_lock, NULL);
    

    /*初始化队列*/
    p_new_tdpl_s->avali_queue_period = thread_num + 1;
    p_new_tdpl_s->avali_queue = malloc((thread_num + 1) * sizeof(struct tdpl_td_handle*));
    p_new_tdpl_s->avali_queue_head = 0;
    p_new_tdpl_s->avali_queue_tail = 1;
    p_new_tdpl_s->call_queue_period = max_wait_n + 1;
    p_new_tdpl_s->call_queue = malloc((max_wait_n + 1) * sizeof(struct tdpl_call_node));
    p_new_tdpl_s->call_queue_head = 0;
    p_new_tdpl_s->call_queue_tail = 1;

    /*启动master线程*/
    //if(pthread_create(&p_new_tdpl_s->master_tid,NULL,tdpl_master_thread,p_new_tdpl_s) == -1){ 
    //    goto err3_ret;
    //}
    /*建立worker线程*/
    for(i = 0;i < thread_num;i++){
        p_td_handle = &p_new_tdpl_s->td_handle_array[i];
        p_td_handle->p_tdpl_s = p_new_tdpl_s; //设置所属线程池
        p_new_tdpl_s->avali_queue[(p_new_tdpl_s->avali_queue_tail++)%p_new_tdpl_s->avali_queue_period] = p_td_handle;
        sem_init(&p_td_handle->run,0,0);//初始化线程继续运行的信号量
        if(pthread_create(&p_td_handle->tid,NULL,tdpl_worker_thread,p_td_handle) == -1){ 
            sem_destroy(&p_td_handle->run);
            goto err4_ret;
        }
    }
    sem_wait(&p_new_tdpl_s->ready_n);  // 等待工作线程建立完毕
    return p_new_tdpl_s;

    /*下面是线程池的创建发生了错误之后需要处理的事情的代码*/

err4_ret:
    while(i-- != 0){
        p_td_handle = &p_new_tdpl_s->td_handle_array[i];
        sem_destroy(&p_td_handle->run);
    }
err3_ret:
    sem_destroy(&p_new_tdpl_s->ready_n);
    sem_destroy(&p_new_tdpl_s->call_wait_n);
    free(p_new_tdpl_s->td_handle_array);
    free(p_new_tdpl_s->avali_queue);
    free(p_new_tdpl_s->call_queue);
err2_ret:
    free(p_new_tdpl_s);
err1_ret:
    return NULL;
}


/* 函数名: int tdpl_call_func(struct tdpl_s *pts,void (*call_func)(void *arg))
 * 功能: 使用线程池的一个worker线程来调用函数
 * 参数: struct tdpl_s *pts,指向线程池结构体的指针
 *       void (*call_func)(void *arg), 需要调用的函数地址
 *       void *arg, 需要调用的函数的参数
 * 返回值: 1,
 *        -1,
 */
int tdpl_call_func(struct tdpl_s *pts, void (*call_func)(void *arg, void *mmpl), void *arg){
    struct tdpl_call_node *p_call_node;

    if(call_func == NULL){
        return -1;
    }

    /*因为没有加锁，所以被判断成满的那一瞬间，队列的节点被消费了*/

    /*请求队列节点入队,需要抢读者锁*/
    //pthread_spin_lock(&pts->call_queue_write_spinlock);
    pthread_mutex_lock(&pts->call_queue_write_lock);
    if(pts->call_queue_head == pts->call_queue_tail){  // 查看请求队列是否已满
        //pthread_spin_unlock(&pts->call_queue_write_spinlock);
        pthread_mutex_unlock(&pts->call_queue_write_lock);
        return -2;  // 如果满则放弃请求
    }
    p_call_node = &pts->call_queue[(pts->call_queue_tail++)%pts->call_queue_period];
    p_call_node->call_func = call_func;
    p_call_node->arg = arg;
    pthread_mutex_unlock(&pts->call_queue_write_lock);
    //pthread_spin_unlock(&pts->call_queue_write_spinlock);
    sem_post(&pts->call_wait_n);  // 告知有调用请求

    return 1;
}

