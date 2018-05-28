#include "semaphore.h"


struct tdpl_td_handle;

/*调用队列的节点*/
struct tdpl_call_node{
    void *arg;
    void (*call_func)(void *arg); //需要调用的函数
};

/*线程池结构体*/
typedef struct tdpl_s{
    int thread_num;  //线程池拥有的线程数
    int max_wait_n;  //最大等待数,或者说是请求队列最大长度
    unsigned long master_tid; //master线程id
    struct tdpl_td_handle *td_handle_array; // 已创建的线程handle结构体数组
    struct tdpl_td_handle **avali_queue;  // 可用线程队列
    struct tdpl_call_node *call_queue; // 调用队列

    /*队列*/
    int call_queue_period;
    unsigned long call_queue_head;  // 调用队列头索引
    unsigned long call_queue_tail;  // 调用队列尾索引
    int avali_queue_period;
    unsigned long avali_queue_head;  // 可用队列头索引
    unsigned long avali_queue_tail;  // 可用队列尾索引

    /*信号量*/
    sem_t ready_n;  // 初始化时用
    sem_t avali_td_n;   //用信号量表示可用线程数
    sem_t avali_queue_write_mutex;
    //用信号量表示当前调用请求等待数, 因为该队列只有master线程读，所以不用加读锁
    sem_t call_wait_n;   
    sem_t call_queue_write_mutex;  // 调用队列写着锁
}*tdpl;

/*线程handle*/
struct tdpl_td_handle{
    struct tdpl_s *p_tdpl_s; //线程所属的线程池的结构体
    unsigned long tid; //线程id
    void (*call_func)(void *arg); //需要调用的函数
    void *arg;  //线程调用函数的参数
    sem_t run;  //用来告知线程开始调用函数
};
/*创建线程池*/
tdpl tdpl_create(int thread_num,int max_wait_n);
/*使用线程池中的一个线程来调用指定的函数*/
int tdpl_call_func(struct tdpl_s *pts,void (*call_func)(void *arg),void *arg);
int tdpl_destroy(struct tdpl_s *pts);
