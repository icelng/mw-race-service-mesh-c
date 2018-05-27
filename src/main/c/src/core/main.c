#include "log.h"
#include "consumer-agent.h"
#include "string.h"
#include "unistd.h"

int main(int argc, char *argv[]){
    log_init(LOG_INFO, "tianch-agent", "./log");
    //log_init(LOG_INFO, "tianch-agent", "/root/logs/agent.log");
    log_info("Hello Tianch!!!!!");

    cagent_start(argc, argv);

    while(1) {
        /*不让主进程退出*/
        sleep(30);
    }
}
