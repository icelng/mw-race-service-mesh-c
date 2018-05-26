#include "log.h"
#include "stdarg.h"
#include "unistd.h"
#include "fcntl.h"
#include "stdio.h"

/*私有全局变量*/
static int g_log_level = 0;


/* 函数名: int log_init(int level, const char *log_ident, const char *log_file_path)
 * 功能: 初始化log日志，进程单例模式，在同一个进程中，只需初始化一次
 * 参数: int level, 日志等级
 *       const char *log_ident, log消息的标识
 *       const char *log_file_path, log输出文件路径
 * 返回值: 1, 初始化成功
 *        <0, 初始化过程中出现了一些问题
 */
int log_init(int level, const char *log_ident, const char *log_file_path){
    int logfd = open(log_file_path, O_RDWR|O_CREAT|O_APPEND,0644);

    g_log_level = level;
    if(logfd == -1){
        return -1;
    }
    close(STDERR_FILENO);
    dup2(logfd,STDERR_FILENO);
    close(logfd);
    openlog(log_ident,LOG_PID|LOG_CONS|LOG_PERROR,0);   
    return 1;
}

/* 函数名: int log_out(int pri, char *fmt, ...)
 * 功能: 输出level级别的log信息
 * 参数: int level, log级别
 *       剩下的参数类似于printf的参数
 * 返回值: 1, 调用成功
 *         0, log级别没有开(不够)
 *        <0, 出现了一些问题
 */
int log_out(int level, char *fmt, ...){
    va_list args;  // 不定参数列表
    if(fmt == NULL){
        /*非法指针*/
        return -1;
    }
    if(level < LOG_EMERG || level > LOG_DEBUG){
        /*log级别非法*/
        return -2;
    }
    va_start(args, fmt);
    if(g_log_level < level){
        /*log级别不够*/
        return 0;
    }
    syslog(LOG_DEBUG, fmt, args);
    va_end(args);
    return 1;
}

/* 函数名: int log_set_level(int level)
 * 功能: 设置log级别
 * 参数: int level, 将要设置的log级别
 * 返回值: 1, 设置成功
 *        <0, 设置出现了一些问题
 */
int log_set_level(int level){
    if(level < LOG_EMERG || level > LOG_DEBUG){
        /*log级别非法*/
        return -1;
    }
    g_log_level = level;
    return 1;
}

/* 函数名: int log_get_level()
 * 功能: 获得log级别
 * 参数: 无
 * 返回值: int, log级别
 */
int log_get_level(){
    return g_log_level;
}

