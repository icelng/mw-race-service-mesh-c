#include "consumer-agent.h"
#include "log.h"
#include "http-server.h"
#include "agent-client-manager.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"
#include "service-discovery.h"
#include "stdlib.h"

int get_parameter_start_index(char *str);
int hash_code(char *str);
void get_service_name(char *src, char *dst_buf);

struct acm_channel *gp_acm_channel;
struct sd_handle *gp_sd_handle;
tdpl g_tdpl_worker[8];
unsigned long call_cnt = 0;

void acm_listening(void *arg, char *data, int data_size){
    struct hs_channel *p_channel = arg;
    char response_body[64];
    //int ret_value = acm_bytes2int(data, 0);
    data[data_size] = 0;

    log_debug("Recv msg:%s from agent-server, data_size:%d", data, data_size);
    data[data_size - 1] = 0;
    sprintf(response_body, "%s", data + 2);

    hs_response_ok(p_channel, response_body, strlen(response_body));
}

void test_response(void *arg, void *mmpl) {
    struct hs_channel *p_hs_channel = arg;

    //int i;
    //int cnt = rand()%60000;  // 防止集中并发
    //for (i = 0;i < cnt;i++);

    usleep(52000);
    hs_response_ok(p_hs_channel, "OK", strlen("OK"));
}


void content_handler(struct hs_channel *p_channel, int content_size, char *content){
    struct acm_channel *p_optimal_agent_channel;
    char service_name[128];

    get_service_name(content, service_name);
    p_optimal_agent_channel = sd_get_optimal_endpoint(gp_sd_handle, service_name); 
    if (p_optimal_agent_channel == NULL) {
        log_err("Failed to get optimal provider-agent!");
        hs_response_ok(p_channel, "Failed!", strlen("Failed!"));
        return;
    }


    int param_start = get_parameter_start_index(content);
    if (acm_request(p_optimal_agent_channel, &content[param_start], content_size - param_start, acm_listening, p_channel) < 0) {
        log_err("Failed to call acm_request!");
        hs_response_ok(p_channel, "Failed!", strlen("Failed!"));
        return;
    }
    
    

    //hs_url_decode(content);
    //param_start = get_parameter_start_index(content);
    //sprintf(response_body, "%d", hash_code(&content[param_start]));

}
 
void cagent_start(int argc, char *argv[]){
    log_info("Hello consumer-agent!");

    struct hs_bootstrap hs_bt;
    struct acm_opt acm_opt;
    struct hs_handle *p_hs_handle;
    struct acm_handle *p_acm_handle;


    acm_opt.io_thread_num = 4;
    acm_opt.worker_thread_num = 8;
    acm_opt.max_hold_req_num = 51200;
    acm_opt.max_write_queue_len = 51200;


    /*启动agent-client-manager*/
    p_acm_handle = acm_start(&acm_opt);

    /*服务发现初始化*/
    log_info("Init service-discovery...");
    //gp_sd_handle = sd_init(p_acm_handle, "http://1g.aliyun:2379");
    gp_sd_handle = sd_init(p_acm_handle, argv[1]);


    /*先初始化内存池*/
    //g_request_tdpl = tdpl_create(256, 512);  // 256个线程  512个等待

    hs_bt.buffer_size = 2048;  // channel的buffer大小为2k,用于读写request reponse
    hs_bt.max_connection = 512;  // 最大连接等待数
    hs_bt.server_port = 20000;  // 端口
    hs_bt.worker_thread_num = 1;  // 工作线程数
    hs_bt.io_thread_num = 2;  // io线程数
    hs_bt.event_loop_num = 1;  // 一个事件循环
    hs_bt.content_handler = content_handler;  // content处理函数

    p_hs_handle = hs_start(&hs_bt);  // 启动http服务器
}

/* 函数名: int get_parameter_start_index(char *str) 
 * 功能: 获得parameter值的首地址(索引),即最后一个等号的下一个字节
 * 参数: char *str,
 * 返回值: 
 */
int get_parameter_start_index(char *str){
    int i;
    int param_start = 0;
    char c;

    for (i = 0;;i++) {
        c = str[i];
        if (c == 0) {
            break;
        }
        if (c == '=') {
            param_start = i + 1;
        }
    }

    return param_start;
}

/* 函数名: int hash_code(char *str) 
 * 功能: 计算hashCode,是java的hashCode
 * 参数: char *str,
 * 返回值: 
 */
int hash_code(char *str){
    int h = 0;
    int i;

    for(i = 0;;i++) {
        if (str[i] == 0) {
            break;
        }
        h = 31*h + str[i];
    }

    return h;
}

int hex2dec(char c){
    if ('0' <= c && c <= '9') {  
        return c - '0';  
    } else if ('a' <= c && c <= 'f') {  
        return c - 'a' + 10;  
    } else if ('A' <= c && c <= 'F') {  
        return c - 'A' + 10;  
    } else {  
        return -1;  
    }  
}

/* 函数名: void get_service_name(char *src, char *dst_buf) 
 * 功能: 从表单里获得服务名,服务名参数必须在首位(有一定的局限性)，顺便进行url解码
 *       必须是表单字符串,不然会出现一些莫名其妙的错误
 * 参数: char *src,
         char *dst_buf,
 * 返回值: 
 */
void get_service_name(char *src, char *dst_buf){
    int i = 0;
    int is_param_start = 0;
    int dst_index = 0;

    for(;;i++) {
        char c = src[i];

        if (c == 0 || c == '&') {
            break;
        }

        if (c == '=') {
            is_param_start = 1;
            continue;
        }
        if (is_param_start == 0) {
            continue;
        }

        if (c != '%') {  
            dst_buf[dst_index++] = c;
        } else {  
            char c1 = src[++i];  
            char c0 = src[++i];  
            int num = 0;  
            num = hex2dec(c1) * 16 + hex2dec(c0);  
            dst_buf[dst_index++] = num;  
        }  
    }
    dst_buf[dst_index] = 0;
}
