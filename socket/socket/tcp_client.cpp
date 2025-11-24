// 1. 必要头文件（比服务器多arpa/inet.h，用于IP地址转换）
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // 提供inet_pton()（将IP字符串转为网络字节序）
using namespace std;
int main(){
    int sock;
    struct sockaddr_in serv_addr;
    const char *send_msg="这是来自客户端的消息";

    sock=socket(AF_INET,SOCK_STREAM,0);
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(8080);
    inet_pton(AF_INET,"127.0.0.1",&serv_addr.sin_addr);
    connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    cout<<"成功连接到服务器"<<endl;

    ssize_t send_len=send(sock,send_msg,strlen(send_msg),0);
    cout<<"消息发送完成，发送字节数："<<send_len<<",内容："<<send_msg<<endl;
    close(sock);
  


    return 0;
}