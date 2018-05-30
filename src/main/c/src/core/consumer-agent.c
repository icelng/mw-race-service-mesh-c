#include "consumer-agent.h"
#include "log.h"
#include "http-server.h"
#include "agent-client-manager.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"

int get_parameter_start_index(char *str);
int hash_code(char *str);

struct acm_channel *gp_acm_channel;

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


void content_handler(struct hs_channel *p_channel, int content_size, char *content){

    if (acm_request(gp_acm_channel, content, content_size, acm_listening, p_channel) < 0) {
        log_err("Failed to call acm_request!");
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

    acm_opt.io_thread_num = 8;
    acm_opt.worker_thread_num = 8;
    acm_opt.max_hold_req_num = 51200;
    acm_opt.max_write_queue_len = 51200;

    /*启动agent-client-manager*/
    p_acm_handle = acm_start(&acm_opt);
    /*连接agent-server*/
    gp_acm_channel = acm_connect(p_acm_handle, "127.0.0.1", 1090);
    if (gp_acm_channel == NULL) {
        log_err("Failed to connect agent-server!");
    }


    /*先初始化内存池*/
    //g_request_tdpl = tdpl_create(256, 512);  // 256个线程  512个等待

    hs_bt.buffer_size = 2048;  // channel的buffer大小为2k,用于读写request reponse
    hs_bt.max_connection = 512;  // 最大连接等待数
    hs_bt.server_port = 20000;  // 端口
    hs_bt.worker_thread_num = 256;  // 工作线程数
    hs_bt.io_thread_num = 16;  // io线程数
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
