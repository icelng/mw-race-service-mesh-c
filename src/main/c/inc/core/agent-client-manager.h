#ifndef AGENT_CLIENT_MANAGER
#define AGENT_CLIENT_MANAGER
#include "mmpool.h"
#include "tdpool.h"
#include "semaphore.h"
#include "pthread.h"


#define ACM_MSG_HEAD_SIZE 12
#define ACM_MAX_EPOLL_EVENTS 16
#define ACM_MAX_CHANNEL_NUM 2048


struct acm_handle {
    tdpl tdpl_io;  // io线程池
    tdpl tdpl_worker;  // worker线程池
    mmpl mmpl;  // 内存池

    int epoll_fd;
    int max_write_queue_len;
    int max_hold_req_num;
};

struct acm_request_map_entry{
    unsigned long req_id;
    void (*listening)(void *arg, char *resp_data, int data_size);
    void *arg;
};

/*报文头*/
struct acm_msg_head {
    long req_id;
    int data_size;
};

/*报文*/
struct acm_msg {
    struct acm_msg_head head;
    struct acm_channel *p_channel;  // 所属channel
    char *data;
};

struct acm_write_task{
    int write_index;
    struct acm_msg_head head;
    char *data;
};

struct acm_channel {
    struct acm_handle* p_handle;
    int socket_fd;  // 客户端套接字描述符
    long request_id;

    unsigned int events;  // channel现在所注册的事件
    struct acm_write_task* write_queue;  // 写请求队列
    /*请求哈希表，因req_id自增，故只要请求量不超过表长度，就不会发生碰撞*/
    struct acm_request_map_entry* request_map;
    unsigned long write_queue_head;
    unsigned long write_queue_tail;
    int is_doing_read;

    /*解码相关*/
    struct acm_msg* recv_msg;  // 接收到的报文
    char head[ACM_MSG_HEAD_SIZE];
    int is_head;
    int head_read_index;
    int data_read_index;

    /*锁或信号量*/
    //sem_t write_queue_mutex;
    pthread_spinlock_t write_queue_spinlock;
    

};

struct acm_opt {
    int max_write_queue_len;  // 最大写请求队列
    int max_hold_req_num;  // 最大挂起请求数
    int io_thread_num;
    int worker_thread_num;
};

struct acm_handle* acm_start(struct acm_opt*);
struct acm_channel* acm_connect(struct acm_handle *p_handle, const char* ip, int port);
int acm_request(struct acm_channel* p_channel, char* buf, int buf_size, void (*listener)(void *arg, char *resp_data, int data_size), void* arg);
unsigned long acm_bytes2long(char* buf, int offset);
void acm_int2bytes(unsigned int value, char* buf, int offset);
void acm_long2bytes(unsigned long value, char* buf, int offset);
unsigned int acm_bytes2int(char* buf, int offset);

#endif
