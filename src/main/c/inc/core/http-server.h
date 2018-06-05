#include "tdpool.h"
#include "mmpool.h"
#include "semaphore.h"

#define MAX_EPOLL_EVENTS 16
#define MAX_IO_THREAD_NUM 64
#define MAX_EPOLL_EVENT_LOOP_NUM 64

#define HTTP_SERVER_RESPONSE_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_SERVER_CONTENT_TYPE "Content-Type:text/html;charset=UTF-8\r\n"

#define HTTP_URL_DECODE_BUFSIZE 2048

struct hs_channel;  // 不完全声明
struct hs_handle;

/*建立链接时所用*/
struct hs_connection_entry {
    struct hs_handle *p_handle;
    int socket_fd;
};

struct hs_handle{
    tdpl tdpl_io;  // IO线程池
    tdpl tdpl_worker;  // 工作线程池
    tdpl tdpl_accept;  // 负责accept
    tdpl tdpl_close;  // 负责关闭链接
    mmpl mmpl;  // 内存池
    int max_connection;
    int buffer_size;
    int sockfd;
    int epoll_fd[MAX_EPOLL_EVENT_LOOP_NUM];  // epoll描述符数组，描述符
    int event_loop_num;
    void (*content_handler)(struct hs_channel *p_channel, int cotent_size, char *content);  // 对content的处理函数
    struct hs_connection_entry *new_connection_map;  // 链接映射，建立新链接时所用
    int new_connection_map_size;
    unsigned long connection_id;  // 链接id建立新链接时所用
};


/*channel，每一条请求链接对应一个*/
struct hs_channel{
    struct hs_handle *p_hs_handle;  // 所属的http-server-handle
    int socket;  // 套接字
    int epoll_fd;  // 所属epoll_fd

    int is_processing;  // 表明是否在处理
    sem_t processing_mutex;  // 处理状态锁，同一个时间只能由一条线程处理

    /*解码所需*/
    int is_line;  // 请求行
    int is_head;  // 表明是否正在解码头部
    int is_body;  // 是否是content
    int is_read_done;  // 是否完成了读操作
    int is_key;  // 解析头时，是否为key
    int is_content_length;
    int key_start;
    int value_start;
    int processing_index;  // 需处理到索引，表明该要处理到的字节
    int processing_index_now;  // 正要处理到的索引
    int decode_index;  // 解码索引

    int body_size;  // content 大小
    int body_start;  // body起始指针

    /*IO所需*/
    int read_index;  //  读索引
    int write_index;  // 写索引，io线程拷贝用
    int write_size;  // 表示要写的字节数
    int buffer_size;  // 缓存大小
    char *buffer;
};

struct hs_bootstrap{
    int server_port;  // 服务器端口
    int io_thread_num;  // io处理线程
    int event_loop_num;  // epoll事件循环线程数
    int worker_thread_num;  // 工作线程
    int max_connection;  // 最大连接数
    int buffer_size;
    void (*content_handler)(struct hs_channel *p_channel, int cotent_size, char *content);  // 对content的处理函数
};


/*启动服务器,返回服务器句柄*/
struct hs_handle* hs_start(struct hs_bootstrap *hs_bt);
int hs_response_ok(struct hs_channel *p_channel, char *response_body, int body_size);
void hs_url_decode(char *url);
