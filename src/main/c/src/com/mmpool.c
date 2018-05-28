#include "mmpool.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"



/* 函数名: int mmpl_create(struct mm_pool_s *new_mmpl)
 * 功能: 创建新的内存池
 * 参数: struct mm_pool_s *new_mmpl,内存池结构体指针
 * 返回值: !NULL, 内存池结构体指针
 *          NULL, 出错了
 */
mmpl mmpl_create(struct mmpl_opt *popt){
    mmpl new_mmpl = (struct mm_pool_s *)malloc(sizeof(struct mm_pool_s));
    memset((void*)new_mmpl,0,sizeof(struct mm_pool_s));
    if(popt == NULL){
        new_mmpl->boundary = MMPL_BOUNDARY_DEFAULT;
        new_mmpl->max_free_index = MMPL_MAX_FREE_INDEX_DEFAULT;

    }else{
        new_mmpl->boundary = popt->boundary;
        new_mmpl->max_free_index = popt->max_free_index;
    }
    new_mmpl->get_cnt = 0;
    new_mmpl->current_free_index = 0;
    new_mmpl->use_head.next = &(new_mmpl->use_head);
    new_mmpl->use_head.pre = &(new_mmpl->use_head);
    sem_init(&new_mmpl->mutex,0,1);  //初始化锁
    return new_mmpl;
}



/* 函数名: int mmpl_destroy(struct mm_pool_s *mmpl)
 * 功能: 销毁内存池，把内存空间归还给操作系统
 * 参数: struct mm_pool_s *mmpl,需要销毁的内存池的结构体
 * 返回值: -1,
 *          1,
 */
int mmpl_destroy(struct mm_pool_s *mmpl){
    int i;
    struct mm_node *p_mm_n,*p_free_h;

    if(mmpl == NULL){
        return -1;
    }
    sem_wait(&mmpl->mutex); //互斥访问内存池
    /*归还正在使用的内存空间给操作系统(在这，如果发现还有正在使用的内存节点，应该报错返回)*/
    while(mmpl->use_head.next != &mmpl->use_head){
        p_mm_n = mmpl->use_head.next;
        mmpl_list_remove(p_mm_n);
        free(p_mm_n);
    }
    /*归还内存池中空闲内存空间给操作系统*/
    for(i = 1;i < MMPL_MAX_INDEX + 1;i++){
        p_free_h = mmpl->free[i];
        if(p_free_h == NULL)continue;
        while(p_free_h->next != p_free_h){
            p_mm_n = p_free_h->next;
            mmpl_list_remove(p_mm_n);
            free(p_mm_n);
        }
        free(p_free_h);
    }
    sem_post(&mmpl->mutex);
    sem_destroy(&mmpl->mutex);
    free(mmpl);
    return 1;
}

/* 函数名: int mmpl_list_insert(struct mm_node *pre_p_n,struct mm_node *p_insert_n)
 * 功能: 把内存节点插入到链表
 * 参数: struct mm_node *pre_p_mm_n,需要插入位置的前驱节点指针
 *       struct mm_node *p_insert_mm_n,需要插入的节点的指针
 * 返回值: -1,
 *          1,
 */
int mmpl_list_insert(struct mm_node *p_pre_n,struct mm_node *p_insert_n){
    if(p_pre_n == NULL || p_insert_n == NULL){
        return -1;
    }
    p_insert_n->next = p_pre_n->next;
    p_insert_n->pre = p_pre_n;
    p_pre_n->next->pre = p_insert_n;
    p_pre_n->next = p_insert_n;
    return 1;
}

/* 函数名: int mmpl_list_remove(struct mm_node *p_rm_node)
 * 功能: 从节点所在的链表中移除该节点
 * 参数: struct mm_node *p_rm_node,需要移除的节点
 * 返回值: -1,
 *          1,
 */
int mmpl_list_remove(struct mm_node *p_rm_node){
    if(p_rm_node->next == p_rm_node){  //如果链表就只有该节点，则无法删除
        return -1;
    }
    p_rm_node->next->pre = p_rm_node->pre;
    p_rm_node->pre->next = p_rm_node->next;
    return 1;
}

/* 函数名: int mmpl_rls_oldestfree(mm_pool_s *mmpl)
 * 功能: 释放最老的空闲内存节点, 注意，调用者要保证线程安全，即需要拿到内存池锁才
 *       能调用该函数
 * 参数: struct mm_pool_s *mmpl
 * 返回值: 1, 释放成功
 *        <0, 释放出现错误
 */
int mmpl_rls_oldestfree(struct mm_pool_s *mmpl){
    unsigned int i, j, min_k, index_temp;
    unsigned int inc_getcnt_array[MMPL_MAX_INDEX];  // 申请次数升序数组
    unsigned long min_getcnt = 0;
    unsigned int oldest_index_array[MMPL_OLDEST_RANGE];
    unsigned int index;
    unsigned int free_node_num, rls_num;  // 空闲内存节点数量，需要释放数量
    struct mm_node *p_mm_n;

    for(i = 0;i < MMPL_MAX_INDEX;i++){
        /*初始化升序数组*/
        inc_getcnt_array[i] = i + 1;
    }
    /*使用冒泡排序，尽管时间复杂度很高，但是MMPL_MAX_INDEX通常很小，所以还是非常非常快的*/
    for(i = 0;i < MMPL_MAX_INDEX;i++){
        min_getcnt = mmpl->latest_get_cnt[inc_getcnt_array[i]];
        min_k = i;
        for(j = i + 1;j < MMPL_MAX_INDEX;j++){
            if(mmpl->latest_get_cnt[inc_getcnt_array[j]] < min_getcnt){
                min_getcnt = mmpl->latest_get_cnt[inc_getcnt_array[j]];
                min_k = j;
            }
        }
        index_temp = inc_getcnt_array[i];
        inc_getcnt_array[i] = inc_getcnt_array[min_k];
        inc_getcnt_array[min_k] = index_temp;
    }
    /*找到存在空闲节点的最久未申请的前MMPL_OLDEST_RANGE的index*/
    j = 0;
    for(i = 0;i < MMPL_OLDEST_RANGE;i++){
        /*如果存在空闲节点的index数量少于MMPL_OLDEST_RANGE*/
        if(j == MMPL_MAX_INDEX){
            /*此时所有的index都找完了，但是拥有空闲内存节点的index数量不足MMPL_OLDEST_RANGE*/
            oldest_index_array[i] = oldest_index_array[i - 1];
            continue;
        }

        while(j < MMPL_MAX_INDEX){
            if(mmpl->free_size[inc_getcnt_array[j]] > 0){  // 存在空闲节点
                oldest_index_array[i] = inc_getcnt_array[j];
                j++;
                break;
            }else{
                j++;
                if(i == 0 && j == MMPL_MAX_INDEX){
                    return -1;  // 全部都空闲的？那肯定是出错的啊！！！
                }
            }
        }
    }
    /*oldest_index_array中每个index对应空闲内存节点都要被释放掉一半的数量*/
    for(i = 0;i < MMPL_OLDEST_RANGE;i++){
        index = oldest_index_array[i];
        free_node_num = mmpl->free_size[index]/index;  // 空闲内存节点数
        if(free_node_num == 0) continue;  // 无需释放
        rls_num = free_node_num/2 + 1;  // 释放一半+1
        for(j = 0;j < rls_num;j++){
            p_mm_n = mmpl->free[index];
            if(p_mm_n->next == p_mm_n){  // 最后一个节点
                mmpl->free[index] = NULL;
                mmpl->free_size[index] = 0;
                mmpl->current_free_index -= index; //空闲内存空间减少
                free(p_mm_n);  // 还给操作系统
            }else{
                mmpl->free[index] = p_mm_n->next;
                mmpl_list_remove(p_mm_n);
                mmpl->free_size[index] -= index;
                mmpl->current_free_index -= index; //空闲内存空间减少
                free(p_mm_n);  // 还给操作系统
            }
        }
    }

    return 1;
}

/* 函数名: void* mmpl_getmem(struct mm_pool_s *mmpl,unsigned int size)
 * 功能: 从指定的内存池里获取到内存空间
 * 参数: struct mm_pool_s *mmpl,内存池结构体指针
 *       int size,需要申请空间的大小
 * 返回值: NULL,获取失败
 *         !=NULL,获取到的内存地址
 */
void* mmpl_getmem(struct mm_pool_s *mmpl,unsigned int size){
    unsigned int align_size;
    unsigned int index;
    struct mm_node *p_mm_n;

    align_size = MMPL_ALIGN(size, mmpl->boundary);  //对齐
    index = align_size/mmpl->boundary;
    if(index > MMPL_MAX_INDEX){  //如果超过MMPL_MAX_INDEX则向操作系统要
        index = 0;
    }

    while (sem_trywait(&mmpl->mutex) < 0);  // 使用自旋锁
    //sem_wait(&mmpl->mutex);  //互斥操作内存池
    if(((p_mm_n = mmpl->free[index]) == NULL) || index == 0){  
        /*如果free数组中没有相应的内存节点，则向操作系统申请*/
        p_mm_n = (struct mm_node *)malloc(align_size + sizeof(struct mm_node));
        if(p_mm_n == NULL){//向操作系统申请内存失败
            sem_post(&mmpl->mutex);
            return NULL;
        }
        p_mm_n->pre=p_mm_n->next = p_mm_n;
        p_mm_n->index = index;
    }else if(p_mm_n->next == p_mm_n){
        /*如果free[index](规则链表)链表只有一个节点*/
        mmpl->free[index] = NULL;
        mmpl->free_size[index] = 0;
        mmpl->current_free_index -= index; //空闲内存空间减少
    }else{
        mmpl->free[index] = p_mm_n->next;
        mmpl_list_remove(p_mm_n);
        mmpl->free_size[index] -= index;
        mmpl->current_free_index -= index; //空闲内存空间减少
    }
    p_mm_n->use_flg = 1; //表示正在被使用
    mmpl_list_insert(&mmpl->use_head,p_mm_n); //插入到使用链表中
    mmpl->get_cnt += 1;  // 申请次数加一
    mmpl->latest_get_cnt[index] = mmpl->get_cnt;  // 记录最近访问统计
    sem_post(&mmpl->mutex);

    return (void *)p_mm_n + sizeof(struct mm_node);
}


/* 函数名: int mmpl_rlsmem(struct mm_pool_s *mmpl,void *rls_mmaddr)
 * 功能: 把从内存池申请到的内存空间还给内存池，如果还给内存池之后空闲的内存空间超
 *       过了内存池所设置的最大空闲内存，则把将要归还给内存池的内存空间直接还给操
 *       作系统。
 * 参数: strcut mm_pool_s *mmpl,内存池
 *       void *rls_mmaddr,需要释放的内存空间的首地址
 * 返回值:
 */
int mmpl_rlsmem(struct mm_pool_s *mmpl,void *rls_mmaddr){
    struct mm_node *p_mm_n;  
    unsigned int index;

    p_mm_n = rls_mmaddr - sizeof(struct mm_node);//获得内存节点的首地址
    if(p_mm_n->use_flg == 0){ //如果本来就是空闲的，则放弃回收
        return -1;
    }
    index = p_mm_n->index;
    sem_wait(&mmpl->mutex);  //互斥操作内存池
    mmpl_list_remove(p_mm_n);  //从使用链表中移除
    p_mm_n->use_flg = 0;
    if(index == 0){  //如果超过了最大index(此时index标为0)，则直接还给操作系统
        free(p_mm_n);
        sem_post(&mmpl->mutex);
        return 1;
    }
    if(mmpl->current_free_index + index > mmpl->max_free_index){
        /*如果归还之后内存池的空闲空间超过了最大空闲空间大小则直接归还给操作系统*/
        free(p_mm_n);
        if(mmpl_rls_oldestfree(mmpl) < 0){  // 并且释放最久没有申请的index对应空闲内存节点 
            sem_post(&mmpl->mutex);
            return -2;
        }
        sem_post(&mmpl->mutex);  //互斥操作内存池
        return 1;
    }
    if(mmpl->free[index] == NULL){
        p_mm_n->pre=p_mm_n->next = p_mm_n;
        mmpl->free[index] = p_mm_n;
    }else{
        if(mmpl_list_insert(mmpl->free[index],p_mm_n) == -1){
            printf("error\n");
        }
    }
    mmpl->free_size[index] += index;
    mmpl->current_free_index += index;
    sem_post(&mmpl->mutex);  //互斥操作内存池
    return 1;
}


