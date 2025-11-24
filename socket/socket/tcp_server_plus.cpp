#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>

using namespace std;
int main(){
    //1.创建监听套接字
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        perror("socket创建错误，终止");
        return -1;
    }
    cout<<"监听套接字创建成功(监听代号:"<<listen<<")"<<endl;

    //2.绑定IP和端口
    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(8090);
    if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr))==-1  ){//疑问：这里强转，为什么不直接(struct sockaddr)server_addr，反而要取地址用指针强转？
        perror("监听和地址绑定失败");
        close(server_fd);//为什么需要手动释放？main结束不会自动释放吗？如果一定要释放，那前面server_addr为什么不需要手动释放？
                                                                            //答：server_fd其实也会主动释放，但是需要养成良好的习惯
        return -1;
    }
    cout<<"绑定8090端口成功"<<endl;

    //3.开始监听(等待连接,触发三次握手)
    if(listen(server_fd,128)==-1){//这里128什么意思
        perror("监听失败");
        close(server_fd);
        return -1;
    }
    //4.接收客户端连接(三次握手之后返回客户端套接字)
    struct sockaddr_in client_addr;
    socklen_t client_addr_len=sizeof(client_addr);
    int client_fd=accept(server_fd,(struct sockaddr *)&client_addr,&client_addr_len);
    if(client_fd==-1){
        perror("接受客户端链接失败");
        close(server_fd);//这里为什么又不需要关闭client_fd？//因为此时client_fd创建其实失败了
        return -1;
    }
            //如果连接成功，打印出连接的客户端的信息
    cout<<"客户端连接成功，ip="<<inet_ntoa(client_addr.sin_addr)<<"，端口="<<ntohs(client_addr.sin_port)<<",客户端标识符="<<client_fd<<",协议类型="<<client_addr.sin_family<<"."<<endl;

    //5.与客户端循环通信
    char recv_buff[1024]={0};
    char send_buff[1024]={0};
                //接受客户端发送的信息
    while(true){
        ssize_t recv_len=recv(client_fd,recv_buff,sizeof(recv_buff)-1,0);//为什么这里需要-1,后面回复不需要-1?
        if(recv_len==-1){
            perror("信息获取失败");
            break;
        }else if(recv_len==0){
            cout<<"客户端断开连接"<<endl;
            break;
        }
        cout<<"收到客户端数据为"<<recv_buff<<endl;
                //回复客户端数据                              //服务端需要有两个fd，本地和客户端，为什么客户端只需要一个fd？不需要记录服务端的信息？
                                                            //另外，是不是服务端也可以扮演客户端的角色？换句话说，两个服务端可以通信？
        cout<<"输入要回复的内容(退出请输入q):"<<endl;
        cin.getline(send_buff,sizeof(send_buff));
        if(strcmp(send_buff,"q")==0){//为什么是等于0//strcmp实际比较的是两个自负的差值，==0意味着相同
            cout<<"服务器主动关闭"<<endl;
            break;
        }
        ssize_t send_len=send(client_fd,send_buff,strlen(send_buff),0);//这里怎么不用sizeof了//只需要发送输入的有效内容
        if(send_len==-1){
            perror("信息回复失败");
            break;
        }
        cout<<"回复成功，发送字节数:"<<send_len<<endl;//这里长度能不能用strlen(send_buff)？
    }
    close(client_fd);
    close(server_fd);
    cout<<"服务器关闭所有连接"<<endl;
    return 0;

}//程序运行成功，但是我发现问题，服务端和客户端相互发消息，最好的方式是你一条我一条，但是如果是一方发多个消息，就不好用了