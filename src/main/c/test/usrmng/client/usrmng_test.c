#include "stdlib.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "encdec.h"
#include "unistd.h"

static char* g_server_rsa_pubkey = "-----BEGIN PUBLIC KEY-----\n\
MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCbOJI6KAN+xVROei9Aspu2VAro\n\
9w1XkiLyS+Jb8BBZ5yw4BUT1mykAbEiVDEzEYkOuTtuyo9uB+U90G3ciI01L+Xx8\n\
ru0Etpy4hH6BO0RihODREokriBcBXoYEB4uYd7hh1Jwz9M+2m0wnBwwQnPTDyjDi\n\
sJO9cCocaJfQV4cczwIDAQAB\n\
-----END PUBLIC KEY-----\n";

/* 函数名: int __snd_msg_test(int sockfd,struct msg *snd_msg)
 * 功能: 通过套接字发送一段数据，为内部调用
 * 参数:
 * 返回值:
 */
int __snd_data(int sockfd,char *data,unsigned short data_size){
    unsigned short snd_size;
    unsigned short real_snd_size;
    union{
        unsigned int u32;
        char u8[4];
    }head;
    const char syc_char = '\r'; //同步符号
    const char ctl_char = 0x10;
    int i;
    snd_size = (data_size + ((1 << 2) - 1)) & ~((1 << 2) - 1);  // 4字节对齐
    head.u32 = snd_size >> 2;
    for(i = 0;i < 5;i++){
        send(sockfd,&syc_char,1,0);  //发送若干个同步符号
    }
    for(i = 0;i < 4;i++){
        if(head.u8[i] == syc_char || head.u8[i] == ctl_char){
            send(sockfd, &ctl_char, 1, 0);
            send(sockfd, &head.u8[i], 1, 0);
        }else{
            send(sockfd, &head.u8[i], 1, 0);
        }
    }
    real_snd_size = send(sockfd,data,snd_size,0); //发送报文
    return 1;
}

int main(){
    struct sockaddr_in servaddr;  
    char plain[4096];
    char cipher[4096];
    int sockfd, cipher_len;
    int test_cnt;

    /* 发送经过公钥加密的用户名密码 */
    encdec_init();
    test_cnt = 0;
    while(1){
        printf("第%d次登录", test_cnt);
        memset(&servaddr,0,sizeof(struct sockaddr_in));
        sockfd = socket(AF_INET,SOCK_STREAM,0);
        servaddr.sin_family = AF_INET;  
        servaddr.sin_port = htons(1080);
        /*把字符串形式的IP转换成标准形式*/
        if(inet_pton(AF_INET, "10.210.81.107", &servaddr.sin_addr) == -1){  
            printf("出错1\n");
        }  
        /*连接服务器*/
        if(connect(sockfd,(struct sockaddr*)&servaddr,sizeof(struct sockaddr_in)) == -1){  
            printf("出错2\n");
        }  
        sprintf(plain, "yiran+hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh+%d", test_cnt++);
        printf("明文长度是:%ld\n", strlen(plain) + 1);
        /* 公钥加密 */
        cipher_len = rsa_pub_encrypt(g_server_rsa_pubkey, (unsigned char *)plain, strlen(plain) + 1, (unsigned char *)cipher, 4096);
        if(cipher_len < 0){
            printf("出错:%s\n", ERR_reason_error_string(ERR_get_error()));
        }
        printf("密文长度:%d\n", cipher_len);
        if(__snd_data(sockfd, cipher, cipher_len) == -1){
            printf("出错4\n");
        }
        fflush(stdout);
        close(sockfd);
    }
    
    return 1;
}
