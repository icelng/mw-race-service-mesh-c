#include "consumer-agent.h"
#include "log.h"
#include "http-server.h"
#include "string.h"
#include "stdio.h"
#include "unistd.h"

int get_parameter_start_index(char *str);
int hash_code(char *str);




void content_handler(struct hs_channel *p_channel, int content_size, char *content){
    char response_body[64];
    int param_start;

    hs_url_decode(content);
    param_start = get_parameter_start_index(content);
    sprintf(response_body, "%d", hash_code(&content[param_start]));

    usleep(50000);
    hs_response_ok(p_channel, response_body, strlen(response_body));
}
 
void cagent_start(int argc, char *argv[]){
    log_info("Hello consumer-agent!");

    struct hs_bootstrap hs_bt;
    struct hs_handle *p_hs_handle;

    /*先初始化内存池*/
    //g_request_tdpl = tdpl_create(256, 512);  // 256个线程  512个等待

    hs_bt.buffer_size = 2048;  // channel的buffer大小为2k,用于读写request reponse
    hs_bt.max_connection = 512;  // 最大链接数
    hs_bt.server_port = 20000;  // 端口
    hs_bt.worker_thread_num = 260;  // 工作线程数
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
