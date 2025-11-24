#include<iostream>
#include<unistd.h>
#include<cstring>
#include<arpa/inet.h>
#include<sys/socket.h>
using namespace std;

int main(){
    //1创建客户端套接字
    int client_fd=socket(AF_INET,SOCK_STREAM,0);
    if(client_fd==-1){
        perror("客户端创建失败");
        close(client_fd);
        return -1;
    }
    cout<<"客户端套接字创建成功，标识符为:"<<client_fd<<endl;
    //2 配置服务器地址
    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    server_addr.sin_port=htons(8090);

    //3 连接服务器
    if(connect(client_fd,(struct sockaddr *)&server_addr,sizeof(server_addr))==-1){
        perror("连接服务器失败");
        close(client_fd);
        return -1;
    }
    cout<<"连接服务器成功,服务器地址：127.0.0.1 8090"<<endl;
    //4 循环通信
    char send_buff[1024];
    char back_buff[1024];

    while(true){
        //发送
        cout<<"请输入要发送给服务器的内容(输入q退出):"<<endl;
        cin.getline(send_buff,sizeof(send_buff));
        if(strcmp(send_buff,"q")==0){
            cout<<"客户端主动结束通信"<<endl;
            break;
        }
        ssize_t send_len=send(client_fd,send_buff,strlen(send_buff),0);
        if(send_len==-1){
            perror("发送信息出现错误");
            break;
        }
        cout<<"成功发送信息，发送字节数为:"<<send_len<<endl;

        //接收
        ssize_t back_len=recv(client_fd,back_buff,sizeof(back_buff)-1,0);
        if(back_len==-1){
            perror("接受信息出现错误");
            break;
        }else if(back_len==0){
            cout<<"服务器断开连接"<<endl;
            break;
        }
        cout<<"收到服务端回复:"<<back_buff<<endl;
    }
    close(client_fd);
    cout<<"客户端关闭"<<endl;
    return 0;
}