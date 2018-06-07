#include "etcdctl.h"
#include "agent-client-manager.h"
#include "semaphore.h"
#include "pthread.h"

#define SERVICE_DISCOVERY_MAX_SERVICE_NUM 1024
#define SERVICE_DISCOVERY_MAX_KEY_LEN 1024
#define SERVICE_DISCOVERY_MAX_LOAD_BALANCE_LIST_LEN 10000


/*服务节点*/
struct sd_endpoint {
    struct acm_channel* p_agent_channel;  // 索引到对应的通道
    int load_level;
    unsigned long remain;  // 剩余请求数

    /*链表相关*/
    struct sd_endpoint *prev;
    struct sd_endpoint *next;
};

struct sd_service_node {
    char* service_name;  // 如果为null表明该节点是空的
    unsigned int hash_code;  // 服务名的hash，用来索引表
    int endpoints_num;  // 节点数
    struct sd_endpoint  *p_next_req_endpoint;  // 下一次请求该选用的endpoint
    struct sd_endpoint *comsuming_list;  // 正在消费的列表
    struct sd_endpoint *saturated_list;  // 饱和的列表

    /*节点单项链表，按load_level大到小排序*/
    struct sd_endpoint* load_balance_list[SERVICE_DISCOVERY_MAX_LOAD_BALANCE_LIST_LEN];
    int lb_list_len;
    unsigned long next_select_ep;
    struct sd_endpoint endpoint_list1_head, endpoint_list2_head;
    //pthread_spinlock_t ep_link_spinlock;  // 节点链表自旋锁
    pthread_rwlock_t lb_list_rwlock;  // 负载均衡列表读写锁
    pthread_mutex_t ep_link_lock;  // 节点链表锁
    //sem_t endpoint_link_mutex;

    /*哈希链表链接*/
    struct sd_service_node *next;
};

struct sd_handle {
    struct etcdctl_handle *etcd_handle;
    struct acm_handle *p_acm_handle;

    struct sd_service_node *service_tb[SERVICE_DISCOVERY_MAX_SERVICE_NUM];  // 服务表,静态
    int service_tb_size;  // 服务表长度
    pthread_rwlock_t service_tb_rwlock;  // 服务表读写锁
    pthread_mutex_t find_mutex;  // 服务发现锁
};

struct sd_handle* sd_init(struct acm_handle *p_acm_handle, const char* etcd_url);
struct acm_channel* sd_get_optimal_endpoint(struct sd_handle *p_handle, char *service_name);
