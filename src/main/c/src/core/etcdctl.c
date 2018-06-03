#include "etcdctl.h"
#include "stdlib.h"
#include "curl/curl.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"
#include "log.h"


size_t etcd_process_get_data(void *buffer, size_t size, size_t nmemb, void *user_p) {
    cJSON **ret_json = (cJSON **)user_p;

    log_debug("ETCD:Get data:%s", buffer);
    *ret_json = cJSON_Parse(buffer);
    if (ret_json == NULL) {
        log_err("ETCD:Failed to parse json:%s", buffer);
    }

    return 0;
}

/* 函数名: struct etcdctl_handle* etcd_init(const char* etcd_url, int port) 
 * 功能: 
 * 参数: struct etcectl_opt *p_opt,
 * 返回值: 
 */
struct etcdctl_handle* etcd_init(const char* etcd_url, int port){
    struct etcdctl_handle *p_new_handle;

    if (etcd_url == NULL) {
        return NULL;
    }

    p_new_handle = malloc(sizeof(struct etcdctl_handle));
    p_new_handle->etcd_url = malloc(strlen(etcd_url) + 1);
    strcpy(p_new_handle->etcd_url, etcd_url);
    p_new_handle->port = port;
    
    return p_new_handle;
}


/* 函数名: cJSON* etcd_get(const char* key_or_dir) 
 * 功能: 获取一个键值对，或者指定目录下的所有键值对
 * 参数: const char* key_or_dir, 键或目录
 * 返回值: 
 */
cJSON* etcd_get(struct etcdctl_handle *p_handle, const char* key_or_dir){
    char curl_url[ETCD_MAX_CURL_URL_LEN];
    cJSON *ret_json = NULL;
    CURLcode ret_code;

    ret_code = curl_global_init(CURL_GLOBAL_SSL);
    if (CURLE_OK != ret_code) {
        log_err("ETCD:Init libcurl failed!");
        return NULL;
    }

    CURL *easy_handle = curl_easy_init();
    if (easy_handle == NULL) {
        log_err("ETCD:Failed to get a easy handle.");
        curl_global_cleanup();
        return NULL;
    }

    sprintf(curl_url, "%s/v2/keys%s", p_handle->etcd_url, key_or_dir);
    log_debug("ETCD:Get %s", curl_url);
    curl_easy_setopt(easy_handle, CURLOPT_URL, curl_url);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, etcd_process_get_data);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &ret_json);

    curl_easy_perform(easy_handle);

    curl_easy_cleanup(easy_handle);
    curl_global_cleanup();
    
    return ret_json;
}
