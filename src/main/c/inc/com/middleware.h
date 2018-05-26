/* 对向中间件链插入中间件或者异常中间件的操作提供了系列函数:
 *  int mw_chain_add();
 *  int mw_chain_exception_add();
 * 以中间件链和首中间数据作为参数执行函数mw_chain_start()即可逐个执行中间件链的中间件。
 * 中间数据的创建、释放和复制必须使用提供的函数来操作:
 * mw_create_middata()/mw_destroy_middata()/mw_copy_middata()
 * 对中间件链执行流控制的操作提供了一系列的宏(在中间件里调用):
 * MIDDLEWARE_NEXT(p_data)
 * 执行下一个中间件，如果当前中间件是末尾中间件，则结束中间件链执行流。必须带上一个参数:
 * 传给下一个中间件的中间数据的首地址，或者NULL。建议传递上一个中间件传来的中间数
 * 据首地址，这样就可以避免内存复制操作，很大地提高性能。如果传递的是当前中间件创
 * 建中间数据，则传递到当前中间件的(在上一个中间件里创建的)中间数据，会在中间件链
 * 处理机里被销毁，无需用户销毁(最好不要自己销毁，不然会出问题的)。
 * MIDDLEWARE_RECALL(p_mdata)
 * 如果是在中间件里调用，则重新调用当前中间件。如果是在异常中间件里调用，则重新执
 * 行产生异常的中间件。其参数与MIDDLEWARE_NEXT()宏的参数类似。
 * MIDDLEWARE_COMPLETE()
 * 结束中间件链的执行流。无参数。
 * MIDDLEWARE_EXCEPTION(type, pmdata)
 * 产生异常，中断当前中间件的执行，继而执行异常中间件。参数:
 * type,异常号，决定调用哪一个异常中间件。
 * pmdata, 中间数据，与宏MIDDLEWARE_NEXT()的参数一样。
 * */
#include "semaphore.h"
#include "tdpool.h"
#include "mmpool.h"

#define MIDDLEWARE_MAX_EXCEPTION_TYPE 256

#define MIDDLEWARE_EVENT_TYPE_EXCEPTION 1
#define MIDDLEWARE_EVENT_TYPE_NEXT 2
#define MIDDLEWARE_EVENT_TYPE_RECALL 3
#define MIDDLEWARE_EVENT_TYPE_COMPLETE 4


#define MIDDLEWARE_DEFINE(middleware_name, pchain, pmdata) \
    struct mw_event middleware_name(struct mw_chain *pchain, struct mw_middata *pmdata){ \
        struct mw_chain *__pthischain = pchain; \
        __pthischain = __pthischain;  /*消除警告*/

#define MIDDLEWARE_EXCEPTION_DEFINE(exception_name, pchain, except_type, sub_type, pmdata) \
    struct mw_event exception_name(struct mw_chain *pchain, unsigned int except_type, unsigned int sub_type, struct mw_middata *pmdata){ \
        struct mw_chain *__pthischain = pchain; \
        __pthischain = __pthischain;  /*消除警告*/

#define MIDDLEWARE_DEFINE_END }



#define MIDDLEWARE_NEXT(p_next_mdata) \
    do{\
        struct mw_event ret_mw_event; \
        ret_mw_event.event_type = MIDDLEWARE_EVENT_TYPE_NEXT; \
        ret_mw_event.pmdata = (p_next_mdata); \
        return ret_mw_event; \
    }while(0)

#define MIDDLEWARE_RECALL(p_next_mdata) \
    do{\
        struct mw_event ret_mw_event; \
        ret_mw_event.event_type = MIDDLEWARE_EVENT_TYPE_RECALL; \
        ret_mw_event.pmdata = (p_next_mdata); \
        return ret_mw_event; \
    }while(0)

#define MIDDLEWARE_COMPLETE() \
    do{ \
        struct mw_event ret_mw_event; \
        ret_mw_event.event_type = MIDDLEWARE_EVENT_TYPE_COMPLETE; \
        ret_mw_event.pmdata = NULL; \
        return ret_mw_event; \
    }while(0)

#define MIDDLEWARE_EXCEPTION(except_type, sub_type_num, p_excep_mdata) \
    do{ \
        struct mw_dispatcher_arg *parg; \
        struct mw_event ret_mw_event; \
        ret_mw_event.pmdata = mw_create_middata(__pthischain, sizeof(struct mw_dispatcher_arg)); \
        parg = ret_mw_event.pmdata->data; \
        parg->sub_type = sub_type_num; \
        parg->exception_type =  except_type; \
        parg->p_exception_mdata = p_excep_mdata; \
        ret_mw_event.event_type = MIDDLEWARE_EVENT_TYPE_EXCEPTION; \
        return ret_mw_event; \
    }while(0)

/*中间之间的中间数据*/
struct mw_middata{
    unsigned int data_size;
    void *data;
};

/*中间件事件*/
/*每一个中间件执行完(结束)了之后，都会产生一个中间件事件，决定着下一步动作*/
/* MIDDLEWARE_EVENT_TYPE_EXCEPTION 表示中间件执行出错，并且结束执行中间件链
 * MIDDLEWARE_EVENT_TYPE_NEXT 表示下来应该执行中间件链中的下一个中间件
 * MIDDLEWARE_EVENT_TYPE_COMPLETE 表示中间件链应该结束了,尽管中间件链还有剩下中间件
 *                             没有执行
 * */
struct mw_event{
    unsigned int event_type;
    struct mw_middata *pmdata;
};

/*异常中间件调度器参数结构体*/
struct mw_dispatcher_arg{
    unsigned int exception_type;  // 异常号
    unsigned int sub_type;  // 异常子号
    struct mw_middata *p_exception_mdata;  // 传给异常中间件处理函数的参数
};

/*中间件初始化选项结构体*/
struct mw_init_opt{

};

struct mw_chain;  // 不完全声明


/*中间件*/
struct middleware{
    struct mw_event (*middleware_func)(struct mw_chain *pchain, struct mw_middata *pmdata);  // 中间件执行函数
    struct middleware *next;  // 下一个中间件
};

/*中间件链表*/
struct mw_list{
    struct middleware head;  // 头结点，也表示异常中间件调度机
    struct middleware *mw_list_tail; // 指向链尾节点的指针
};
/*中间件链*/
struct mw_chain{
    struct mw_list mw_list;  // 指向中间件链表
    struct mw_middata *p_first_mdata;  // 首个middata, 作为第一个中间件执行函数的参数
    /*异常中间体执行函数数组, 并且，需要拿到中间件链表的写者锁才能对其进行修改*/
    struct mw_event (*mw_exception_func_array[MIDDLEWARE_MAX_EXCEPTION_TYPE])(struct mw_chain *pchain, unsigned int, unsigned int, struct mw_middata*);  
    unsigned int read_cnt;  // 读者写着问题所需要的阅读计数
    sem_t read_mutex;  // 读者锁
    sem_t write_mutex;  // 写者锁
    tdpl tdpl;  // 线程池
    mmpl mmpl;  // 内存池
};



/*中间件链选项结构体*/
struct mw_chain_opt{

};


struct mw_chain* mw_create_chain(struct mw_chain_opt*);
int mw_destroy_chain(struct mw_chain *pchain);
struct mw_middata* mw_create_middata(struct mw_chain*, unsigned int data_size);
int mw_destroy_middata(struct mw_chain*, struct mw_middata *pmdata);
struct mw_middata* mw_copy_middata(struct mw_chain*, struct mw_middata *pmdata);
int mw_chain_init(struct mw_chain *chain);
int mw_chain_add(struct mw_chain *chain, struct mw_event (*func)(struct mw_chain *pchain, struct mw_middata *pmdatqa));
int mw_chain_exception_add(struct mw_chain *pchain, unsigned int exception_type, struct mw_event (*exception_func)(struct mw_chain*, unsigned int, unsigned int, struct mw_middata*));
int mw_chain_start(struct mw_chain *chain, struct mw_middata *mdata);
