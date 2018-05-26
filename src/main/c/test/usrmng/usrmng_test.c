#include "usrmng.h"
#include "stdio.h"
#include "log.h"
#include "unistd.h"

int main(){
    log_init(LOG_DEBUG, "usrmng_test", "./test.log");
    log_out(LOG_DEBUG, "Hello! usrmng test!!!");
    usrmng_init("./config.json");
    while(1){
        sleep(1);
    }
    return 1;
}

