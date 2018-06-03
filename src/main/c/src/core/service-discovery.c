#include "service-discovery.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "log.h"

char* g_root_path = "dubbomesh";

/* 函数名: int sd_insert_endpoint(struct sd_endpoint *p_head, struct sd_endpoint *p_endpoint) 
 * 功能: 给指定的服务插入节点
 * 参数: struct sd_service_node *p_service_node,
         struct sd_endpoint *p_endpoint,
 * 返回值: 
 */
int sd_insert_endpoint(struct sd_endpoint *p_head, struct sd_endpoint *p_will_add){

    if (p_head == NULL || p_will_add == NULL) {
        return -1;
    }

    struct sd_endpoint *pre, *now;;

    pre = p_head;
    now = pre->next;
    while (now != p_head) {
        if (p_will_add->load_level > now->load_level) {
            break;
        }
        pre = now;
        now = now->next;
    }
    /*插入到now的前面*/
    p_will_add->remain = p_will_add->load_level;
    p_will_add->next = now;
    pre->next = p_will_add;
    p_will_add->prev = pre;
    now->prev = p_will_add;
    
    return 1;
}

/* 函数名: int sd_hash_code(char *str) 
 * 功能: 计算hashCode,是java的hashCode
 * 参数: char *str,
 * 返回值: 
 */
unsigned int sd_hash_code(char *str){
    unsigned int h = 0;
    int i;

    for(i = 0;;i++) {
        if (str[i] == 0) {
            break;
        }
        h = 31*h + (unsigned char)str[i];
    }

    return h;
}

/* 函数名: int sd_get_or_add_service(struct sd_handle *p_handle, const char* service_name) 
 * 功能: 添加服务
 * 参数: const char* service_name,
 * 返回值: 
 */
struct sd_service_node* sd_get_or_add_service(struct sd_handle *p_handle, char* service_name){
    unsigned int hash_code = sd_hash_code(service_name);
    struct sd_service_node *p_service_node = NULL, *p = NULL;

    int tb_entry_index = hash_code % p_handle->service_tb_size;

    /*检查是否存在*/
    p = p_handle->service_tb[tb_entry_index];
    while (p != NULL) {
        if (!strcmp(p->service_name, service_name)) {
            /*存在*/
            return p;
        }
        p = p->next;
    }

    /*服务初始化*/
    p_service_node = malloc(sizeof(struct sd_service_node));
    p_service_node->service_name = malloc(strlen(service_name) + 1);
    strcpy(p_service_node->service_name, service_name);
    p_service_node->hash_code = hash_code;
    p_service_node->endpoints_num = 0;
    p_service_node->comsuming_list = &p_service_node->endpoint_list1_head;
    p_service_node->endpoint_list1_head.next = &p_service_node->endpoint_list1_head;
    p_service_node->endpoint_list1_head.prev = &p_service_node->endpoint_list1_head;
    p_service_node->saturated_list = &p_service_node->endpoint_list2_head;
    p_service_node->endpoint_list2_head.next = &p_service_node->endpoint_list2_head;
    p_service_node->endpoint_list2_head.prev = &p_service_node->endpoint_list2_head;
    p_service_node->p_next_req_endpoint = p_service_node->comsuming_list;
    sem_init(&p_service_node->endpoint_link_mutex, 0, 1);

    /*链入链表*/
    if (p_handle->service_tb[tb_entry_index] == NULL) {
        p_service_node->next = NULL;
    } else {
        p_service_node->next = p_handle->service_tb[tb_entry_index];
    }
    p_handle->service_tb[tb_entry_index] = p_service_node;
    
    return p_service_node;
}

/* 函数名: struct sd_service_node* sd_get_service_node(struct sd_handle *p_handle, char* service_name) 
 * 功能: 获得服务表中的服务表项
 * 参数: struct sd_handle *p_handle,
         char* service_name,
 * 返回值: 
 */
struct sd_service_node* sd_get_service_node(struct sd_handle *p_handle, char* service_name){
    unsigned int hash_code = sd_hash_code(service_name);
    struct sd_service_node *p = NULL;

    int tb_entry_index = hash_code % p_handle->service_tb_size;
    p = p_handle->service_tb[tb_entry_index];
    if (p == NULL) {
        return NULL;
    }

    while (p != NULL) {
        if (!strcmp(p->service_name, service_name)) {
            break;
        }
        p = p->next;
    }
    
    return p;
}


/* 函数名: struct sd_handle* sd_init(const char* etcd_url) 
 * 功能: 初始化服务发现
 * 参数: const char* etcd_url, etcd地址
 * 返回值: 
 */
struct sd_handle* sd_init(struct acm_handle *p_acm_handle, const char* etcd_url){
    struct sd_handle *p_new_handle;
    int i;

    if (etcd_url == NULL) {
        goto err1_ret;
    }
    
    p_new_handle = malloc(sizeof(struct sd_handle));
    if (p_new_handle == NULL) {
        goto err1_ret;
    }

    /*初始化服务表*/
    p_new_handle->service_tb_size = SERVICE_DISCOVERY_MAX_SERVICE_NUM;
    sem_init(&p_new_handle->service_tb_mutex, 0, 1);  // 初始化服务表锁
    for (i = 0;i < p_new_handle->service_tb_size;i++) {
        p_new_handle->service_tb[i] = NULL;
    }

    /*etcd*/
    log_info("SERVICE FIND:Init etcd on url:%s", etcd_url);
    p_new_handle->etcd_handle = etcd_init(etcd_url, 0);
    if (p_new_handle->etcd_handle == NULL) {
        goto err2_ret;
    }

    /*acm*/
    p_new_handle->p_acm_handle = p_acm_handle;
    
    return p_new_handle;

err2_ret:
    free(p_new_handle);
err1_ret:
    return NULL;
}

/* 函数名: int sd_rls_endpoints(struct sd_service_node p_service_node) 
 * 功能: 释放节点
 * 参数: struct sd_service_node p_service_node,
 * 返回值: 
 */
int sd_rls_endpoints(struct sd_service_node *p_service_node){
    struct sd_endpoint *p_ep, *q;

    p_ep = p_service_node->comsuming_list->next;
    while(p_ep->next != p_ep) {
        /*非头结点*/
        q = p_ep;
        p_ep = p_ep->next;
        free(q);
    }
    p_ep->prev = p_ep;

    p_ep = p_service_node->saturated_list->next;
    while(p_ep->next != p_ep) {
        /*非头结点*/
        q = p_ep;
        p_ep = p_ep->next;
        free(q);
    }
    p_ep->prev = p_ep;

    p_service_node->p_next_req_endpoint = p_service_node->comsuming_list;
    p_service_node->endpoints_num = 0;
    
    return 1;
}

/* 函数名: int sd_parse_ip_and_port(char *src, char *ip_buf, int *port) 
 * 功能: 解析ip和端口
 * 参数: char *src,
         char *ip_buf,
         int *port,
 * 返回值: 
 */
int sd_parse_ip_and_port(char *src, char *ip_buf, int *port){
    char c;
    int i = strlen(src);

    do {
        c = src[i--];
        if (c == ':') {
            *port = atoi(&src[i + 2]);
            src[i + 1] = 0;
        }
        if (c == '/') {
            strcpy(ip_buf, &src[i+2]);
            break;
        }
    } while(i != 0);
    
    return 1;
}


/* 函数名: int sd_service_find(struct sd_handle *p_handle, char* service_name) 
 * 功能: 服务发现,向etcd查询服务节点
 * 参数: struct sd_handle *p_handle,
         char* service_name,
 * 返回值: 
 */
int sd_service_find(struct sd_handle *p_handle, char* service_name){
    int ret_value = 1;
    char etcd_key[SERVICE_DISCOVERY_MAX_KEY_LEN];

    struct sd_service_node *p_service_node = sd_get_or_add_service(p_handle, service_name);
    if (p_service_node == NULL) {
        ret_value = -4;
        goto err1_ret;
    }
    /*每次服务发现都释放节点*/
    log_info("SERVICE FIND:Release old endpoints.");
    sem_wait(&p_service_node->endpoint_link_mutex);
    sd_rls_endpoints(p_service_node);
    sem_post(&p_service_node->endpoint_link_mutex);

    /*etcd*/
    log_info("SERVICE FIND:Geting service node from etcd....");
    sprintf(etcd_key, "/%s/%s", g_root_path, service_name);
    cJSON *etcd_json = etcd_get(p_handle->etcd_handle, etcd_key);
    /*解析json*/
    if (etcd_json == NULL) {
        ret_value = -1;
        goto err1_ret;
    }
    log_debug("SERVICE_DISCOVERY:Find the node");
    cJSON *node = cJSON_GetObjectItemCaseSensitive(etcd_json, "node");
    if (node == NULL) {
        ret_value = -2;
        goto err2_ret;
    }
    log_debug("SERVICE_DISCOVERY:Find the nodes");
    cJSON *nodes = cJSON_GetObjectItemCaseSensitive(node, "nodes");
    if (nodes == NULL) {
        ret_value = -3;
        goto err2_ret;
    }
    log_debug("SERVICE_DISCOVERY:Find the each node for nodes.");
    cJSON_ArrayForEach(node, nodes) {
        cJSON *key = cJSON_GetObjectItemCaseSensitive(node, "key");
        cJSON *value = cJSON_GetObjectItemCaseSensitive(node, "value");
        char ip[16];
        int port;

        log_info("SERVICE_FIND:Find the key:%s, value:%s", key->valuestring, value->valuestring);

        if (cJSON_IsNull(key) || cJSON_IsNull(value)) {
            ret_value = -5;
            goto err2_ret;
        }

        sd_parse_ip_and_port(key->valuestring, ip, &port);
        log_info("SERVICE_FIND:Find the endpoint ip:%s, port:%d", ip, port);
        struct acm_channel *p_new_channel = acm_connect(p_handle->p_acm_handle, ip, port);
        if (p_new_channel == NULL) {
            log_err("SERIVE_FIND:Failed to connect provider-agent!");
            continue;
        }
        struct sd_endpoint *p_new_endpoint = malloc(sizeof(struct sd_endpoint));
        p_new_endpoint->load_level = atoi(value->valuestring);
        p_new_endpoint->p_agent_channel = p_new_channel;
        p_new_endpoint->remain = p_new_endpoint->load_level;
        sd_insert_endpoint(p_service_node->comsuming_list, p_new_endpoint);
    }
    p_service_node->p_next_req_endpoint = p_service_node->comsuming_list->next;
    log_info("SERVICE_DISCOVERY:Service found complete.");

err2_ret:
    cJSON_Delete(etcd_json);
err1_ret:
    return ret_value;
}

/* 函数名: struct acm_channel* sd_get_optimal_endpoint(struct sd_handle *p_handle, char *service_name) 
 * 功能: 获得最优节点 
 * 参数: struct sd_handle *p_handle,
         char *service_name,
 * 返回值: 
 */
struct acm_channel* sd_get_optimal_endpoint(struct sd_handle *p_handle, char *service_name){
    struct sd_service_node *p_service_node = sd_get_service_node(p_handle, service_name);
    if (p_service_node == NULL) {
        /*进行服务发现*/
        sem_wait(&p_handle->service_tb_mutex);
        p_service_node = sd_get_service_node(p_handle, service_name);
        if (p_service_node == NULL) {
            log_info("SERVICE FIND:Find the service:%s", service_name);
            if (sd_service_find(p_handle, service_name) < 0) {
                log_err("SERIVCE_FIND:Failed to find the service:%s", service_name);
                sem_post(&p_handle->service_tb_mutex);
                return NULL;
            }
            p_service_node = sd_get_service_node(p_handle, service_name);
        }
        sem_post(&p_handle->service_tb_mutex);
    }

    sem_wait(&p_service_node->endpoint_link_mutex);
    struct sd_endpoint *p_endpoint = p_service_node->p_next_req_endpoint;
    if (p_endpoint == p_service_node->comsuming_list) {
        /*如果是头节点*/
        log_debug("SERVICE_DISCOVERY:Is head node");
        p_endpoint = p_endpoint->next;
        if (p_endpoint == p_service_node->comsuming_list) {
            log_debug("SERVICE_DISCOVERY:Is also head node");
            /*还是头结点，说明消费链表已空，则交换链表(饱和链表转换成消费链表)*/
            p_service_node->comsuming_list = p_service_node->saturated_list;
            p_service_node->saturated_list = p_endpoint;
            p_endpoint = p_service_node->comsuming_list->next;
        }
    }
    p_service_node->p_next_req_endpoint = p_endpoint->next;
    if (--p_endpoint->remain == 0) {
        /*如果消费完了，则转移节点到饱和链表*/
        p_endpoint->prev->next = p_endpoint->next;
        p_endpoint->next->prev = p_endpoint->prev;
        sd_insert_endpoint(p_service_node->saturated_list, p_endpoint);
    }
    sem_post(&p_service_node->endpoint_link_mutex);

    return p_endpoint->p_agent_channel;
}
