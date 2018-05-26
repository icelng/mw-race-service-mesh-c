/* 单例模式，在每个进程里，只执行一次uds_init()即可使用
 * */
#include "ipc-uds.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "stddef.h"
#include "errno.h"
#include "string.h"
#include "sys/socket.h"
#include "sys/un.h"
#include "sys/stat.h"
#include "sys/time.h"
#include "fcntl.h"
#include "mmpool.h"


static mmpl g_uds_mmpl;  // 使用到的内存池


/* 函数名: int uds_init(uds_init_opt *popt)
 * 功能: 初始化uds
 * 参数: uds_init_opt *popt, 初始化选项结构体指针
 * 返回值: 1, 初始化成功
 *        <0, 初始化失败
 */
int uds_init(struct uds_init_opt *popt){
    if(popt == NULL){
        g_uds_mmpl = mmpl_create(NULL);  // 使用默认选项的内存池
        if(g_uds_mmpl == NULL){
            return -1;
        }
    }else{

    }
    return 1;
}



/* 函数名: int uds_listen(char *servername)
 * 功能: 创建unix domain socket,并且作为服务器进行监听
 * 参数: char *servername,表示socket的文件路径
 *       int max_connection_num, 表示最大的连接数
 * 返回值: >0, unix domain socket 描述符
 *         <0, 出错
 *          
 */
int uds_listen(char *servername, int max_connection_num){
    int fd;
    int err;
    int len,ret_val;  // ret_val为返回值
    struct sockaddr_un un;
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        return -1;
    }
    unlink(servername);  // 如果文件存在，则删除文件
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;  // 作一些常规的配置
    strcpy(un.sun_path, servername);  // 复制文件的路径
    len = offsetof(struct sockaddr_un, sun_path) + strlen(servername);  // 计算该结构体共占用的空间
    if(bind(fd, (struct sockaddr *)&un, len) < 0){  // 绑定套接字
        ret_val = -2;
    }else{
        if(listen(fd, max_connection_num) < 0){  // 绑定成功之后开始监听
            ret_val = -3;
        }else{
            return fd;
        }
    }
    err = errno;  // 防止执行close(fd)之后会覆盖errno
    close(fd);  // 此时此刻应该是绑定失败的了, 关闭套接字借口
    errno = err;
    return ret_val;
}


/* 函数名: int uds_accept(int listen_fd, uid_t *puid)
 * 功能: 接收unix socket连接请求
 * 参数: int listen_fd 监听连接请求的socket描述符
 *       uid_t *puid, 进程id的指针，用来获得请求连接的进程的pid
 * 返回值:
 */
int uds_accept(int listen_fd, uid_t *puid){
    int client_socket_fd, ret_val;
    int err;
    unsigned int len;
    struct sockaddr_un un;
    struct stat statbuf;
    len = sizeof(un);
    /*接收unix socket的连接请求*/
    if((client_socket_fd = accept(listen_fd, (struct sockaddr *)&un, &len)) < 0){
        ret_val = -1;
    }
    /*下面开始获得请求连接的进程pid*/
    len -= offsetof(struct sockaddr_un, sun_path);  // 取得路径的长度
    un.sun_path[len] = 0;  // 添加上字符串的结尾字符
    if(stat(un.sun_path, &statbuf) < 0){  // 获取套接字文件的状态
        ret_val = -2;
    }else{
        if(S_ISSOCK(statbuf.st_mode)){
            if(puid != NULL) *puid = statbuf.st_uid;  // 获取pid
            unlink(un.sun_path);
            return client_socket_fd;
        }else{
            ret_val = -3;
        }
    }
    err = errno;
    close(client_socket_fd);
    errno = err;
    return ret_val;
}

/* 函数名: int uds_connect(char *servername)
 * 功能: 套接字连接 
 * 参数: char *servername， unix socket 对应的文件路径
 * 返回值: >0, unix socket 描述符
 *         <0, 出错了
 */
int uds_connect(char *servername){
    int fd;
    int err;
    int len,ret_val;  // ret_val为返回值
    struct sockaddr_un un;
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        return -1;
    }
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;  // 作一些常规的配置
    sprintf(un.sun_path, "socktmp%05d", getpid());
    len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);  // 计算该结构体共占用的空间
    if(bind(fd, (struct sockaddr *)&un, len) < 0){  // 绑定套接字
        ret_val = -2;
    }else{
        /*连接服务端*/
        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, servername);
        len = offsetof(struct sockaddr_un, sun_path) + strlen(servername);
        if(connect(fd, (struct sockaddr *)&un, len) < 0){  // 开始连接
            ret_val = -3;
        }else{
            return fd;  // 连接成功
        }
    }
    /*如果程序执行到了这里，说明程序执行出错了*/
    err = errno;
    close(fd);  // 关闭套接字
    errno = err;
    return ret_val;
}

/* 函数名: void uds_close(int fd)
 * 功能: 关闭unix socket
 * 参数: int fd, 套接字描述符
 * 返回值: 无
 */
void uds_close(int fd){
    close(fd);
}

/* 函数名: int uds_create_umsg(void *data, unsigned int data_size)
 * 功能: 创建unix套接字传送单元包, 注意了，新建立的umsg中*data所指的数据是从参数
 *       *data所指的数据复制来的，pumsg->data与函数参数data不是同一个值(指针)
 * 参数: void *data, 数据首地址
 *       unsigned int data_size, 数据的大小
 * 返回值: !NULL, 创建成功
 *         NULL, 创建失败
 */
struct uds_umsg* uds_create_umsg(void *data,unsigned int data_size, unsigned int type){
    int errno_temp;
    struct uds_umsg *pumsg = NULL;
    pumsg = (struct uds_umsg *)mmpl_getmem(g_uds_mmpl, sizeof(struct uds_umsg) + data_size);  // 向内存池申请内存
    //pumsg = (uds_umsg *)malloc(sizeof(uds_umsg));
    if(pumsg == NULL){
        errno_temp =  errno;
        goto ret_err1;
    }
    pumsg->data = ((void *)pumsg) + sizeof(struct uds_umsg);
    if(data != NULL){
        memcpy(pumsg->data, data, data_size);
    }
    pumsg->data_size = data_size;
    pumsg->data_type = type;
    return pumsg;

ret_err1:
    errno = errno_temp;
    return NULL;
}


/* 函数名: int uds_free_umsg(uds_umsg *umsg)
 * 功能: 释放unix套接字传送单元包
 * 参数: uds_umsg *umsg, 指向数据单元包结构体的指针
 * 返回值: 1, 释放成功
 *        <0, 释放失败
 */
int uds_free_umsg(struct uds_umsg *pumsg){
    if(pumsg == NULL){
        return -1;
    }
    if(mmpl_rlsmem(g_uds_mmpl, pumsg) < 0)
        return -2;
    return 1;
}


/* 函数名: int uds_send(int usockfd, uds_umsg *umsg)
 * 功能: 通过unix套接字发送数据
 * 参数: int usockfd, 套接字描述符
 * 返回值: 1, 发送成功
 *        <0, 发送失败
 */
int uds_send(int usockfd, struct uds_umsg *pumsg){
    unsigned int i;
    uds_umsg_h_un head;
    const char syc_char = '\r'; //同步符号
    const char ctl_char = 0x10;
    if(pumsg == NULL){
        return -1;
    }
    head.umsg_h.data_size = pumsg->data_size;
    head.umsg_h.data_type = pumsg->data_type;
    for(i = 0;i < 5;i++){
        if(1 != send(usockfd,&syc_char,1,0)) return -2;  //发送若干个同步符号
    }
    for(i = 0;i < sizeof(struct uds_umsg_h);i++){
        if(head.u8[i] == syc_char || head.u8[i] == ctl_char){
            if(1 != send(usockfd,&ctl_char,1,0)) return -3;
            if(1 != send(usockfd,&head.u8[i],1,0)) return -4;
        }else{
            if(1 != send(usockfd,&head.u8[i],1,0)) return -5;
        }
    }
    if(pumsg->data_size != send(usockfd, pumsg->data, pumsg->data_size, 0)){
        return -6;
    }
    return 1;
}


/* 函数名: socket_umsg* uds_recv(int usockfd)
 * 功能: 等待接收来自unix套接字的数据, 接收到的umsg得要自己释放
 * 参数: int usockfd, 套接字描述符
 * 返回值: !NULL, socket_umsg的指针
 *          NULL, 接收出现错误
 */
struct uds_umsg* uds_recv(int usockfd){
    struct uds_umsg *p_new_umsg = NULL;
    struct timeval timeout;   //设置超时用
    int syc_cnt = 0;  //同步计数
    int monitor_cnt = 0; //监听字符计数
    int h_i = 0; //接收head的index
    uds_umsg_h_un head;  // 发送头, 控制发送和接收
    const char syc_c = '\r';  //同步字符
    const char ctl_c = 0x10;  //控制字符
    char rcv_c;
    int rcv_status = 0;//0,处于监听状态，1处于报文接收状态

    timeout.tv_sec = 5;  //5秒钟超时
    timeout.tv_usec = 0;
    //设置接收超时
    setsockopt(usockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));

    while(1){
        if(rcv_status == 0){  //处于报文接收状态
            if(monitor_cnt++ == 50){ //如果监听的字符计数超过了50则退出
                return NULL;
            }
            if(recv(usockfd,&rcv_c,1,0) == -1){
                return NULL;
            }
            if(rcv_c == syc_c){
                if(++syc_cnt == 3){  //开始接收head
                    while(1){
                        if(recv(usockfd,&rcv_c,1,0) == -1){
                            return NULL;
                        }
                        if(rcv_c == syc_c)continue;
                        if(rcv_c == ctl_c){
                            if(recv(usockfd,&rcv_c,1,0) == -1){
                                return NULL;
                            }
                        }
                        head.u8[h_i++] = rcv_c;
                        if(h_i == sizeof(struct uds_umsg_h)){  // 头的大小
                            rcv_status = 1;
                            break;
                        }
                    }
                }
            }else{
                syc_cnt = 0;
            }
        }else if(rcv_status == 1){
            p_new_umsg = uds_create_umsg(NULL, head.umsg_h.data_size, 0);
            if(p_new_umsg == NULL){
                return NULL;
            }
            p_new_umsg->data_size = head.umsg_h.data_size;
            p_new_umsg->data_type = head.umsg_h.data_type;
            switch(p_new_umsg->data_type){  // 根据类型选择接受方式
                case UMSG_TYPE_FD:  // 如果收到的是文件描述符
                    if(uds_rcvfd(usockfd, (int *)(p_new_umsg->data)) < 0){
                        uds_free_umsg(p_new_umsg);
                        return NULL;
                    }
                    break;
                default:
                    if(recv(usockfd,p_new_umsg->data,p_new_umsg->data_size,0) != p_new_umsg->data_size){
                        uds_free_umsg(p_new_umsg);
                        return NULL;
                    }
                    break;
            }
            return p_new_umsg;
        }
    }
    return NULL;
}



/* 函数名: int uds_sendfd(int usockfd, int fd)
 * 功能: 通过unix套接字发送文件描述符
 * 参数: int usockfd, unix套接字描述符
 *       int fd, 需要发送的文件描述符
 * 返回值: 1, 发送成功
 *        <0, 发送失败
 */
int _uds_sendfd(int usockfd, int fd){
    struct msghdr msg;
    struct iovec iov[1];
    unsigned char buf_temp;
    union{
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    }control_un;
    struct cmsghdr *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(cmptr)) = fd;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = &buf_temp;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    if(-1 == sendmsg(usockfd, &msg, 0)){
        return -1;
    }
    return 1;
}

/* 函数名: int uds_sendfd(int usockfd, int sfd)
 * 功能: 通过unix套接字发送文件描述符
 * 参数: int usockfd, unix套接字描述符
 *       int sfd, 需要发送的描述符
 * 返回值: 1, 发送成功
 *        <0, 发送失败
 */
int uds_sendfd(int usockfd, int sfd){
    unsigned int i;
    uds_umsg_h_un head;
    const char syc_char = '\r'; //同步符号
    const char ctl_char = 0x10;
    
    head.umsg_h.data_size = sizeof(int);
    head.umsg_h.data_type = UMSG_TYPE_FD;
    for(i = 0;i < 5;i++){
        if(1 != send(usockfd,&syc_char,1,0)) return -2;  //发送若干个同步符号
    }
    for(i = 0;i < sizeof(struct uds_umsg_h);i++){  // 逐字节发送头
        if(head.u8[i] == syc_char || head.u8[i] == ctl_char){
            if(1 != send(usockfd,&ctl_char,1,0)) return -3;
            if(1 != send(usockfd,&head.u8[i],1,0)) return -4;
        }else{
            if(1 != send(usockfd,&head.u8[i],1,0)) return -5;
        }
    }
    if(_uds_sendfd(usockfd, sfd) < 0){
        return -6;
    }

    return 1;
}

/* 函数名: int uds_rcvfd(int usockfd, int *pfd)
 * 功能: 接收通过unix套接字发送的文件描述符
 * 参数: int usockfd, unix套接字描述符
 *       int *pfd, 指向接受文件描述符变量的指针
 * 返回值: 1, 接受成功
 *        <0, 接收失败
 */
int uds_rcvfd(int usockfd, int *pfd){
    struct msghdr msg;
    struct iovec iov[1];
    char buf_temp;
    ssize_t n;
    int newfd;
    union{
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    }control_un;
    struct cmsghdr *cmptr;
    iov[0].iov_base = &buf_temp;
    iov[0].iov_len = 1;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if((n = recvmsg(usockfd, &msg, 0)) <= 0){
        *pfd = -1;
        return -1;
    }
    if((cmptr = CMSG_FIRSTHDR(&msg)) != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(int))){
        if(cmptr->cmsg_level != SOL_SOCKET){
            *pfd = -1;
            return -2;
        }
        if(cmptr->cmsg_type != SCM_RIGHTS){
            *pfd = -1;
            return -3;
        }
        *pfd = *((int *)CMSG_DATA(cmptr));
    }else{
        *pfd = -1;
        return -4;
    }
    return 1;
}


