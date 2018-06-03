#include "etcdctl.h"
#include "agent-client-manager.h"
#include "semaphore.h"

#define SERVICE_DISCOVERY_MAX_SERVICE_NUM 1024
#define SERVICE_DISCOVERY_MAX_KEY_LEN 1024


/*服务节点*/
struct sd_endpoint {
    void* p_agent_channel;  // 索引到对应的通道
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
    struct sd_endpoint endpoint_list1_head, endpoint_list2_head;
    sem_t endpoint_link_mutex;

    /*哈希链表链接*/
    struct sd_service_node *next;
};

struct sd_handle {
    struct etcdctl_handle *etcd_handle;
    struct acm_handle *p_acm_handle;

    struct sd_service_node *service_tb[SERVICE_DISCOVERY_MAX_SERVICE_NUM];  // 服务表,静态
    int service_tb_size;  // 服务表长度
    sem_t service_tb_mutex;  // 服务表锁
};

struct sd_handle* sd_init(struct acm_handle *p_acm_handle, const char* etcd_url);
struct acm_channel* sd_get_optimal_endpoint(struct sd_handle *p_handle, char *service_name);
