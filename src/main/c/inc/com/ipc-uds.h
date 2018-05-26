#include "sys/socket.h"


#define UMSG_TYPE_FD 1


/*unix socket传输的单元数据包*/
struct uds_umsg{
    int data_type;  // 数据类型
    unsigned int data_size;  // data大小
    void *data;  // 数据
};  //umsg = unit message

/*发送的头，用来控制发送的*/
struct uds_umsg_h{
    int data_type;
    unsigned int data_size;
};



/*初始化选项结构体*/
struct uds_init_opt{

};

typedef union{
    struct uds_umsg_h umsg_h;
    unsigned char u8[sizeof(struct uds_umsg_h)];
}uds_umsg_h_un;

int uds_init(struct uds_init_opt*);
int uds_listen(char *servername, int max_connection_num);
int uds_accept(int listen_fd, uid_t *puid);
int uds_connect(char *servername);
void uds_close(int fd);
struct uds_umsg* uds_create_umsg(void *data, unsigned int data_size, unsigned int type);
int uds_free_umsg(struct uds_umsg *umsg);
int uds_send(int usockfd, struct uds_umsg *umsg);
struct uds_umsg* uds_recv(int usockfd);
int uds_sendfd(int usockfd, int sfd);
int uds_rcvfd(int usockfd, int *pfd);
