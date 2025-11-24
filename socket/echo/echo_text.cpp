//制作一个建议echo服务(多线程)
/*核心流程：
1、创建socket
2、bind
3、listen
4、accept
5、创建线程(用通信fd和客户端交互)
6、循环accept，处理多个客户端


*/
#include<stdio.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
//什么是主线程，main和进程什么关系？
#define port 8080
#define buff_size 1024
//线程处理函数：和单个客户端通信
void * handle_client(void * arg){
    int connect_fd=*(int *)arg;
    free(arg);//为什么需要我主动释放？主线程只负责分配，所以释放的责任给到了执行线程函数的线程这里

    pthread_detach(pthread_self());//分离线程的作用：把线程状态设为 detached（分离状态），线程结束后，内核会自动回收它的所有资源，不需要主线程 join
//所以说一般的线程资源是依靠join函数来回收吗？
    char buff[buff_size];
    while(1){
        ssize_t n=read(connect_fd,buff,sizeof(buff)-1);//留一个位置放'\0',这里n是读到的字节数
        if(n<=0){
            if(n==0){
                printf("客户端断开连接\n");
            }else 
                perror("read 失败");
            break;
        }
        buff[n]='\0';
        printf("收到客户端消息:%s\n",buff);

        ssize_t ret_msg=write(connect_fd,buff,n);//这里n是写的字节数,这里只是把数据写到connect_fd，有发送给原客户端吗？
        //这里只要写入到connect_fd，内核会自动发送
        //注意，读connect_fd和写connect_fd不是同一个！一个是读缓冲区，一个是写缓冲区
        if(ret_msg==-1){
            perror("回写失败\n");
            break;
        }
    }//这里为什么没有关闭=（exit）？//因为exit会终止整个进程和他的线程，单纯终止一个线程只需要执行return即可

    close(connect_fd);//这里的connect_fd是int类型？这样真的能释放吗，不会出现格式错误吗？//socket()\accept()的返回值都是int类型的fd
    return NULL;//为什么返回NULL？
}

int main(){
    //1 创建tcp socket
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd==-1){
        perror("socket创建失败");
        return 1;
    }
    int opt =1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //2绑定端口
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;

    if(bind(listen_fd,(struct sockaddr *)&addr,sizeof(addr))==-1){
        perror("bind失败");
        close(listen_fd);
        return 1;
    }

    //3、开始监听
    if(listen(listen_fd,8)==-1){
        perror("监听失败");
        close(listen_fd);
        return 1;
    }
    printf("服务器启动，监听开始%d……\n",port);
    //4、循环接收客户端链接
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);
        //accept
        int * connect_fd=(int *)malloc(sizeof(int));
        *connect_fd=accept(listen_fd,(struct sockaddr *)&client_addr,&client_len);//在accept之前，client_addr和client_len是否明确？//accept返回的是什么？
        //初始化的是存储地址的缓冲区大小！初始化的本质是：告诉内核我给你的缓冲区有多大
        if(*connect_fd==-1){
            perror("accept失败");
            free(connect_fd);
            continue;
        }
        printf("新客户端链接\n");
        //创建线程处理该客户端

        pthread_t tid;
        if(pthread_create(&tid,NULL,handle_client,connect_fd)!=0){
            perror("线程创建失败");
            close(*connect_fd);
            free(connect_fd);//为什么close后面还要free？

        }

    }
    close(listen_fd);
    return 0;

}