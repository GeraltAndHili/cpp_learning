#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netinet/in.h>
using namespace std;
int main(){
    int server_fd;
    int new_socket;
    struct sockaddr_in address;//这里sockaddr_in是关键字还是临时取得名字? //是ipv4专用的地址结构体类型
    int opt=1;
    socklen_t addrlen=sizeof(address);
    char buffer[1024]={0};

    server_fd=socket(AF_INET,SOCK_STREAM,0);
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt));

    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(8080);

    bind(server_fd,(struct sockaddr*)&address,sizeof(address));//sockaddr_in和sockaddr的区别，前者是ipv4专用，后者是通用类型
    listen(server_fd,3);
    cout<<"服务器启动：监听8080端口，等待客户端连接"<<endl;

    struct sockaddr_in c_adress;
    socklen_t client_addr_len=sizeof(c_adress);

    new_socket=accept(server_fd,(struct sockaddr*)&c_adress,&client_addr_len);
    cout<<"客户端连接成功，客户端ip:"<<inet_ntoa(address.sin_addr)<<",端口："<<ntohs(address.sin_port)<<endl;
    ssize_t valread=read(new_socket,buffer,1024);//这里也可以用recv，recv是专门用来读取socket的
    cout<<"收到客户端消息:("<<valread<<"字节)"<<buffer<<endl;
    close(new_socket);
    close(server_fd);
    return 0;
}