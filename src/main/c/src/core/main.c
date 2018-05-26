#include "log.h"
#include "consumer-agent.h"
#include "string.h"
#include "unistd.h"

int main(int argc, char *argv[]){
    log_init(LOG_DEBUG, "tianch-agent", "./debug.out");
    log_info("Hello Tianch!!!!!");

    if (argc == 1) {
        log_err("请添加启动参数!");
        return 1;
    }
    if (!strcmp(argv[1], "consumer")) {
        /*启动consumer*/
        cagent_start(argc - 2, &argv[2]);
    } else if(!strcmp(argv[1], "provider")) {
        /*启动provider*/
    } else {
        log_err("请启动consumer或者provider");
        return 1;
    }

    while(1) {
        /*不让主进程退出*/
        sleep(30);
    }
}
