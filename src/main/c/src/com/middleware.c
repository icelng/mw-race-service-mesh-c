#include "middleware.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"



//static tdpl g_mw_tdpl;  // 全局变量，中间件所使用的线程池
//static mmpl g_mw_mmpl;  // 全局变量，内存池

/* 函数名: int mw_init(struct mw_init_opt *popt)
 * 功能: 初始化中间件机制，使得进程支持中间件的调用, 一个进程只能调用一次
 * 参数: struct mw_init_opt *popt, 选项结构体指针
 * 返回值: 1, 初始化成功
 *        <0, 初始化失败
 */
//int mw_init(struct mw_init_opt *popt){
//    struct mmpl_opt mmpl_opt;
//    if(popt == NULL){
//        /*默认选项*/
//        g_mw_tdpl = tdpl_create(20, 100);  // 启动20个线程， 最大请求等待数100
//        mmpl_opt.boundary = MMPL_BOUNDARY_1K;  // 1K对齐
//        mmpl_opt.max_free_index = 2000;  // 最大空闲2M
//        g_mw_mmpl = mmpl_create(&mmpl_opt);  // 默认配置内存池
//
//    }else{
//
//    }
//    return 1;
//}


/* 函数名: struct mw_middata* mw_create_middata(unsigned int data_size)
 * 功能: 创建middata,内存由内存池提供
 * 参数: unsigned int data_size, 数据的大小
 * 返回值: !NULL, 为新创建的middata的首地址
 *          NULL, 创建过程中出现了问题
 */
struct mw_middata* mw_create_middata(struct mw_chain *pchain, unsigned int data_size){
    struct mw_middata *p_new_mdata;
    if(pchain == NULL){
        /*传入的是非法中间件链*/
        return NULL;
    }
    p_new_mdata = mmpl_getmem(pchain->mmpl, sizeof(struct mw_middata) + data_size);
    if(p_new_mdata == NULL){
        return NULL;
    }
    p_new_mdata->data = (void *)p_new_mdata + sizeof(struct mw_middata);
    p_new_mdata->data_size = data_size;
    return p_new_mdata;
}

/* 函数名: int mw_destroy_middata(struct mw_middata *pmdata);
 * 功能: 销毁middata, 把middata占用的空间归还给内存池
 * 参数: struct mw_middata *pmdata, 需要销毁的middata的首地址
 * 返回值: 1, 销毁成功
 *        <0, 销毁失败
 */
int mw_destroy_middata(struct mw_chain *pchain, struct mw_middata *pmdata){
    if(pchain == NULL){
        /*传入了非法的中间件链*/
        return -1;
    }
    if(pmdata == NULL){
        return -2;  // 传入的是非法指针
    }
    if(mmpl_rlsmem(pchain->mmpl, pmdata) < 0){
        /*内存归还给内存池失败*/
        return -3;
    }
    return 1;
}

/* 函数名: struct mw_middata* mw_copy_middata(struct mw_middata *pmdata)
 * 功能: 复制middata,其实就是向内存池多申请空间，保存middata的副本
 * 参数: struct mw_middata *pmdata, 需要被复制的middata
 * 返回值: !NULL, 复制成功
 *          NULL, 复制失败
 */
struct mw_middata* mw_copy_middata(struct mw_chain *pchain, struct mw_middata *pmdata){
    struct mw_middata *p_new_mdata;
    if(pchain == NULL){
        /*传入了非法的中间件链*/
        return NULL;
    }
    if(pmdata == NULL){
        return NULL;  // 传入的是非法指针
    }
    p_new_mdata = mmpl_getmem(pchain->mmpl, sizeof(struct mw_middata) + pmdata->data_size);
    if(p_new_mdata == NULL){
        return NULL;
    }
    p_new_mdata->data_size = pmdata->data_size;
    p_new_mdata->data = (void *)p_new_mdata + sizeof(struct mw_middata);
    memcpy(p_new_mdata->data, pmdata->data, pmdata->data_size);
    return p_new_mdata;
}

/* 函数名: struct mw_event mw_exception_dispatcher(struct mw_middata *pmdata)
 * 功能: 异常中间件调度机
 * 参数: struct mw_middata *pmdata, 中间数据
 * 返回值: struct mw_event, 执行结束之后所触发的事件
 */
struct mw_event mw_exception_dispatcher(struct mw_chain *pchain, struct mw_middata *pmdata){
    struct mw_event ret_mw_event;  // 需要返回给中间件处理机的事件结构体
    struct mw_dispatcher_arg *parg;  // 指向中间件处理机传来的参数
    ret_mw_event.pmdata = NULL;  // 默认空
    parg = pmdata->data;
    if(pchain->mw_exception_func_array[parg->exception_type] != NULL){
        /*如果对应的异常号的异常中间件有被注册,则执行异常中间体*/
        ret_mw_event = (pchain->mw_exception_func_array[parg->exception_type])(pchain, parg->exception_type, parg->sub_type, parg->p_exception_mdata);
    }
    if(parg->p_exception_mdata != ret_mw_event.pmdata && ret_mw_event.event_type != MIDDLEWARE_EVENT_TYPE_EXCEPTION){
        /* 传给异常中间件的参数应该由异常中间件调度机销毁，该参数(中间数据)是在
         * 发生异常的中间件中由用户创建的。而传给该中间件调度机的参数(中间数据)
         * 是在发生异常的中间件中由宏创建(非用户创建),但可让中间件链处理机统一视
         * 为中间件产生的中间数据进行销毁。
         * */
        mw_destroy_middata(pchain, parg->p_exception_mdata);
    }else if(ret_mw_event.event_type == MIDDLEWARE_EVENT_TYPE_EXCEPTION && parg->p_exception_mdata != ((struct mw_dispatcher_arg*)ret_mw_event.pmdata->data)->p_exception_mdata){
        /* 异常中间件产生了异常，并且下次异常中间件与刚执行完的异常中间件的参数(
         * 中间数据)不同，所以得要销毁上一次的参数(中间数据)。
         * */
        mw_destroy_middata(pchain, parg->p_exception_mdata);
    }
    return ret_mw_event;
}

/* 函数名: struct mw_chain* mw_create_chain(struct mw_chain_opt *popt)
 * 功能: 创建中间件链
 * 参数: struct mw_chain_opt *popt, 中间件链选项结构体指针
 * 返回值: !NULL, 创建成功，返回中间件链描述结构体首地址，所有对中间件链的操作都
 *                需要其作为参数来进行。
 *          NULL, 创建失败
 */
struct mw_chain* mw_create_chain(struct mw_chain_opt *popt){
    struct mw_chain *p_new_chain = NULL;
    struct mmpl_opt mmpl_opt;
    int i;
    /*对于中间件链描述结构体所占的内存，直接向操作系统申请。*/
    p_new_chain = malloc(sizeof(struct mw_chain));
    if(p_new_chain == NULL){
        /*内存申请失败*/
        return NULL;
    }
    /*初始化中间件链表*/
    /*中间件链表头作为异常中间件调度器*/
    p_new_chain->mw_list.head.next = NULL;
    p_new_chain->mw_list.head.middleware_func = mw_exception_dispatcher;
    p_new_chain->mw_list.mw_list_tail = &p_new_chain->mw_list.head;  // 链尾指向头
    /*初始化首中间数据*/
    p_new_chain->p_first_mdata = NULL;  // 为空
    /*初始化异常中间件数组*/
    for(i = 0;i < MIDDLEWARE_MAX_EXCEPTION_TYPE;i++){
        p_new_chain->mw_exception_func_array[i] = NULL;
    }
    /*初始化读者写者模型锁*/
    p_new_chain->read_cnt = 0;  // 读者数量计数变量
    sem_init(&p_new_chain->read_mutex, 0, 1);  // 初始化读者锁
    sem_init(&p_new_chain->write_mutex, 0, 1);  // 初始化写者锁
    /*下面初始化内存池和线程池(可配置)*/
    if(popt == NULL){
        /* 默认选项*/
        p_new_chain->tdpl = tdpl_create(20, 100);  // 启动50个线程，最大100个等待
        if(p_new_chain->tdpl == NULL){
            /*线程池创建失败*/
            return NULL;
        }
        mmpl_opt.boundary = MMPL_BOUNDARY_1K;  // 1K对齐
        mmpl_opt.max_free_index = 2000;  // 最大空闲2M
        p_new_chain->mmpl = mmpl_create(&mmpl_opt);  // 默认配置内存池
        if(p_new_chain->mmpl == NULL){
            /*内存池创建失败*/
            return NULL;
        }
    }else{
        /*用户选项*/
    }
    return p_new_chain;
}


/* 函数名: int mw_destroy_chain(struct mw_chain *pchain)
 * 功能: 销毁中间件链，把中间件链占用的相关资源给释放掉。
 * 参数: struct mw_chain *pchain, 中间件链描述结构体的指针
 * 返回值: 1, 销毁成功
 *        <0, 销毁失败
 */
int mw_destroy_chain(struct mw_chain *pchain){
    struct middleware *p, *p_free;
    if(pchain == NULL){
        return -1;
    }
    /*销毁中间件链相关资源之前，需要拿到写者锁。等待中间件链执行完毕才继续。*/
    sem_wait(&pchain->write_mutex);
    /*1.因为中间件链表中每一个中间件的描述结构体的内存都是通过malloc操作申请的，
     * 所以在这里得要挨个地free掉。
     * */
    p = pchain->mw_list.head.next;
    while(p != NULL){
        p_free = p;
        p = p->next;
        free(p_free);
    }
    /*2.销毁线程池*/
    if(tdpl_destroy(pchain->tdpl) < 0){
        return -2;
    }
    /*3.销毁内存池*/
    if(mmpl_destroy(pchain->mmpl) < 0){
        return -3;
    }
    sem_post(&pchain->write_mutex);
    /*4.释放锁*/
    sem_destroy(&pchain->write_mutex);
    sem_destroy(&pchain->read_mutex);
    /*5.中间链相关资源被释放之后，最后就释放掉中间件链描述结构体。*/
    free(pchain);
    return 1;
}


/* 函数名: int mw_chain_init(struct mw_chain *chain)
 * 功能: 初始化中间件链
 * 参数: mw_chain *chain, 指向中间件链结构体
 * 返回值: 1, 初始化成功
 *        <0, 初始化出现了一些问题
 *        
 */
//int mw_chain_init(struct mw_chain *chain){
//    int i;
//    if(g_mw_mmpl == NULL){
//        /*中间件机制(内存池)没有初始化*/
//        return -1;
//    }
//    if(chain == NULL){
//        return -2;
//    }
//    /* 向内存池申请内存(一次申请), 然后把空间分别分配给中间件链表信息结构体和错误
//     * 中间件处理函数数组。
//     * */
//    chain->p_mw_list = mmpl_getmem(g_mw_mmpl, sizeof(struct mw_list) + sizeof(void *) * MIDDLEWARE_MAX_EXCEPTION_TYPE);
//    chain->mw_exception_func_array = (void*)(chain->p_mw_list) + sizeof(struct mw_list);
//    if(chain->p_mw_list == NULL){
//        /*申请内存失败*/
//        return -3;
//    }
//    /*初始化错误中间件处理函数数组*/
//    for(i = 0;i < MIDDLEWARE_MAX_EXCEPTION_TYPE;i++){
//        chain->mw_exception_func_array[i] = NULL;
//    }
//    chain->p_mw_list->head.middleware_func = mw_exception_dispatcher;  // 头节点（错误中间件）
//    chain->p_mw_list->head.next = NULL;  // 初始化链头
//    chain->p_mw_list->mw_list_tail = &chain->p_mw_list->head;  // 链尾指针指向链头
//    chain->p_mw_list->read_cnt = 0;  // 初始化"阅读量"
//    sem_init(&chain->p_mw_list->read_mutex, 0, 1);  // 初始化读者锁
//    sem_init(&chain->p_mw_list->write_mutex, 0, 1);  // 初始化写者锁
//}


/* 函数名: int mw_chain_add(struct mw_chain *chain, struct mw_event* (*func)(struct mw_middata *pmdatqa))
 * 功能: 给中间件链添加中间件
 * 参数: struct mw_chain *chain， 指向中间件链的指针
 *       struct mw_event* (*func)(struct mw_middata *pmdatqa), 中间件执行函数
 * 返回值: 1, 添加成功
 *        <0, 添加失败
 */
int mw_chain_add(struct mw_chain *pchain, struct mw_event (*func)(struct mw_chain *pchain, struct mw_middata *pmdatqa)){
    struct middleware *new_middleware;

    if(pchain == NULL || func == NULL){  // 如果参数都是空的，出错，返回
        return -1;
    }
    /* 一个中间件链就几十来个中间件，并且中间件所需内存申请释放频次不高，所以就没
     * 必要用内存池了吧?目前所设计的内存池片内碎片可是很高的！！！
     * */
    new_middleware = (struct middleware*)malloc(sizeof(struct middleware));
    if(new_middleware == NULL){  // 如果申请内存失败，则返回-2
        return -2;
    }
    new_middleware->middleware_func = func;  // 关联上中间件处理函数
    /*把中间件链入到链尾*/
    new_middleware->next = NULL;
    sem_wait(&pchain->write_mutex);  // 需要拿到写者锁才能进行修改
    pchain->mw_list.mw_list_tail->next = new_middleware;
    pchain->mw_list.mw_list_tail = new_middleware;
    sem_post(&pchain->write_mutex);
    return 1;
}

/* 函数名: int mw_chain_exception_add(struct mw_chain *pchain, unsigned int exception_type, struct mw_event (*exception_func)(unsigned int, struct mw_middata))
 * 功能: 添加异常中间件
 * 参数: struct mw_chain *pchain, 中间件链
 *       unsigned int exception_type, 异常类型
 *       struct mw_event (*exception_func)(unsigned int, struct mw_middata), 异常中间件处理函数
 * 返回值: 1, 添加成功
 *        <0, 添加过程出现了问题
 */
int mw_chain_exception_add(struct mw_chain *pchain, unsigned int exception_type, struct mw_event (*exception_func)(struct mw_chain*, unsigned int, unsigned int, struct mw_middata*)){
    if(pchain == NULL || exception_func == NULL){
        /*参数是空的，出错*/
        return -1;
    }
    sem_wait(&pchain->write_mutex);  // 需要拿到链表写者锁才能进行添加操作
    pchain->mw_exception_func_array[exception_type] = exception_func;
    sem_post(&pchain->write_mutex);  // 需要拿到链表写者锁才能进行添加操作
    return 1;
}


/* 函数名: void mw_chain_handler()
 * 功能: 中间件链处理机, 其单独运行于一个线程中, 负责挨个执行中间件链中的中间件处
 *       理函数。
 * 参数: void *pvoid_chain, 中间件链数据结构指针
 * 返回值: 无
 */
void mw_chain_handler(void *pvoid_chain){
    struct mw_chain *pchain = pvoid_chain;  // 指向中间件链
    struct mw_middata *pmdata = NULL;  // 指向中间数据
    struct middleware *p_now_middleware = NULL;  // 指向当前中间件
    struct middleware *p_exception_dispatcher = NULL;  // 指向异常中间件调度机
    struct mw_event mw_event;
    int next_middleware_flag = 0;
    sem_wait(&pchain->read_mutex);
    if(pchain->read_cnt++ == 0){  
        /*如果是第一个读者，则需拿起写者锁才能够进行"阅读"(中间件)*/
        /*第一个读者拿到写者锁之后，下来的读者无需拿到写者锁即可阅读*/
        sem_wait(&pchain->write_mutex); 
    }
    sem_post(&pchain->read_mutex);

    /*只要还有中间件链处理机(线程)正在执行以下代码，中间件链是无法被修改的*/
    pmdata = pchain->p_first_mdata;
    p_now_middleware = pchain->mw_list.head.next;
    p_exception_dispatcher = &pchain->mw_list.head;
    while(p_now_middleware != NULL){
        mw_event = p_now_middleware->middleware_func(pchain, pmdata);  // 执行中间件处理函数
        if(pmdata != mw_event.pmdata && mw_event.event_type != MIDDLEWARE_EVENT_TYPE_EXCEPTION){
            /*如果在刚执行完的中间体中，产生了新的中间数据*/
            mw_destroy_middata(pchain, pmdata);  // 销毁上一个中间数据，可以是空，但不能是非法的
        }else if(mw_event.event_type == MIDDLEWARE_EVENT_TYPE_EXCEPTION && pmdata != ((struct mw_dispatcher_arg*)mw_event.pmdata->data)->p_exception_mdata){
            /* 虽然是产生了异常，但是需要传给异常异常中间件的中间数据不等于上一个
             * 中间数据，还是需要销毁上一个中间数据的*/
            mw_destroy_middata(pchain, pmdata);  // 销毁上一个中间数据，可以是空，但不能是非法的
        }
        pmdata = mw_event.pmdata;  // 获得刚执行结束的中间件所产生的中间数据
        do{
            switch(mw_event.event_type){
                case MIDDLEWARE_EVENT_TYPE_EXCEPTION:
                    mw_event = p_exception_dispatcher->middleware_func(pchain, pmdata);  // 执行异常中间件调度器
                    mw_destroy_middata(pchain, pmdata);  // 销毁上一个中间数据
                    pmdata = mw_event.pmdata;
                    next_middleware_flag = 0;  // 返回再次判断
                    break;
                case MIDDLEWARE_EVENT_TYPE_NEXT:
                    p_now_middleware = p_now_middleware->next;
                    next_middleware_flag = 1;
                    break;
                case MIDDLEWARE_EVENT_TYPE_RECALL:
                    next_middleware_flag = 1;
                    break;
                case MIDDLEWARE_EVENT_TYPE_COMPLETE:
                    p_now_middleware = NULL;  // 结束中间件链
                    next_middleware_flag = 1;
                    break;
            }
        }while(next_middleware_flag != 1);
    }
    if(pmdata != NULL){
        /*如果最后一个中间件产生了中间数据，则需要销毁*/
        mw_destroy_middata(pchain, pmdata);
    }


    /*只要还有中间件链处理机(线程)正在执行以上代码，中间件链是无法被修改的*/

    sem_wait(&pchain->read_mutex);
    if(--pchain->read_cnt == 0){
        /*如果是最后一个读者放下"书本",则释放掉写者锁，允许写者对"书本"进行修改*/
        sem_post(&pchain->write_mutex);
    }
    sem_post(&pchain->read_mutex);
    
}


/* 函数名: int mw_chain_start(struct mw_chain *chain)
 * 功能: 开始执行中间件链的中间体
 * 参数: struct mw_chain *chain, 指向中间件链的指针
 *       struct mw_middata *mdata, 第一个middata, 作为第一个中间件执行函数的参数
 * 返回值: 1, 开始执行成功
 *        <0, 开始执行出现了一些问题
 */
int mw_chain_start(struct mw_chain *pchain, struct mw_middata *pmdata){
    if(pchain == NULL){
        /*传入非法指针*/
        return -2;
    }
    /*多拷贝一份中间数据，为了让用户更能感觉是在异步调用中间件链*/
    /*如果不在这里多拷贝一份中间数据，而直接用传过来指针指向的数据，那么在第一中
     *间使用该中间数据之前，如果该中间数据被用户修改了，很可能就会出现奇奇怪怪的
     *问题。
     * */
    pchain->p_first_mdata = mw_copy_middata(pchain, pmdata);  
    if(pchain->p_first_mdata == NULL && pmdata != NULL){
        /*拷贝失败*/
        return -4;
    }
    /*调用线程池的线程，从首中间体开始执行*/
    if(tdpl_call_func(pchain->tdpl, mw_chain_handler, pchain, sizeof(struct mw_chain)) < 0){  
        return -5;
    }

    return 1;
}
