#include "cJSON.h"


#define ETCD_MAX_CURL_URL_LEN 1024

struct etcdctl_handle {
    char *etcd_url;
    int port;
};

struct etcdctl_opt {

};


struct etcdctl_handle* etcd_init(const char* etcd_url, int port);
cJSON* etcd_get(struct etcdctl_handle *p_handle, const char* key_or_dir);

