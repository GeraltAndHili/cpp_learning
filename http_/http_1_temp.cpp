#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<sys/socket.h>
#include<cjson/cJSON.h>
//注意：网络编程中，包括很多其他场景，错误处理本身就是逻辑的一部分！！！！！

using namespace std;

//全局变量定义
const int Port=8080;
const int Buff_size=4096;
const int max_clients=128;
void trim(char* str) {
    if (str == NULL || *str == '\0') return;

    // 去除头部空白
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }

    // 去除尾部空白
    char* end = start + strlen(start) - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }

    // 给字符串添加结束符
    *(end + 1) = '\0';
    // 复制处理后的字符串到原地址
    strcpy(str, start);
}
// 12. 工具函数：根据文件路径后缀返回Content-Type
const char* get_content_type(const char* path) {
    if (strstr(path, ".html") != NULL) {
        return "text/html; charset=utf-8";
    } else if (strstr(path, ".css") != NULL) {
        return "text/css; charset=utf-8";
    } else if (strstr(path, ".js") != NULL) {
        return "application/javascript; charset=utf-8";
    } else if (strstr(path, ".json") != NULL) {
        return "application/json; charset=utf-8";
    } else if (strstr(path, ".png") != NULL) {
        return "image/png";
    } else if (strstr(path, ".jpg") != NULL || strstr(path, ".jpeg") != NULL) {
        return "image/jpeg";
    } else {
        return "text/plain; charset=utf-8";  // 默认类型
    }
}
//工具函数,根据请求路径返回对应的http响应
void send_http_response(int client_fd,const char * status,const char * content_type,const char* body){//这些参数都什么意思
    char response[Buff_size]={0};
    sprintf(response,
         "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: Close\r\n"  // 告诉客户端，响应后关闭连接
            "\r\n"
            "%s",
            status,
            content_type,
            strlen(body),
            body
    );
    send(client_fd,response,strlen(response),0);
}
//线程函数声明(多线程处理客户端)
void * client_handler(void *argv){
    int client_fd=*(int *)argv;
    delete(int *)argv;//这里为什么要手动删除，为什么要强转？
    //接受客户端的http请求
    char recv_buff[Buff_size]={0};
    ssize_t recv_len=recv(client_fd,recv_buff,sizeof(recv_buff)-1,0);
    //打印请求
    cout<<"收到http请求：\n"<<recv_buff<<endl;
    //解析请求
    char method[64]={0};//请求方法
    char path[256]={0};//请求路径

    sscanf(recv_buff,"%s %s %*s",method,path);//这里什么意思，为什么要用sscanf？不能直接赋值吗
    trim(method);
    trim(path);

    cout<<"解析http请求：方法="<<method<<",路径="<<path<<endl;
    //-------------------------------实现http接口-----------------------
    if(strcmp(method,"GET")==0&&strcmp(path,"/hello")==0){//处理hello
        const char* body="你好，这里是http服务器";
        send_http_response(client_fd,"200 OK","text/plain;charset=utf-8",body);
    }else if(strcmp(method,"GET")==0&&strcmp(path,"/user")==0){//处理/user
        cJSON * root=cJSON_CreateObject();
        cJSON_AddStringToObject(root,"username","吴金朋");
        cJSON_AddNumberToObject(root,"age",28);
        cJSON_AddStringToObject(root,"gender","男");
        cJSON_AddStringToObject(root,"message","这是来自http服务器的json响应");

        char * json_body=cJSON_Print(root);
        cJSON_Delete(root);//为啥要先转换再释放？

        send_http_response(client_fd,"200 OK","application/json;charset=utf-8",json_body);
        free(json_body);//函数结束不是会自动释放吗?
    }
    // 新增：处理GET /index.html接口（返回静态HTML文件）
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/index.html") == 0) {
    // 打开本地HTML文件（当前目录下创建index.html）
    FILE* file = fopen("index.html", "r");
    if (file == NULL) {  // 文件不存在，返回404
        const char* body = "404 Not Found! index.html不存在";
        send_http_response(client_fd, "404 Not Found", get_content_type(path), body);
    } else {
        // 读取文件内容
        char file_buf[Buff_size] = {0};
        size_t file_len = fread(file_buf, 1, sizeof(file_buf)-1, file);
        fclose(file);

        // 发送HTML响应（Content-Type自动判断）
        send_http_response(client_fd, "200 OK", get_content_type(path), file_buf);
    }
    }else{
        const char * body="404 Not Found!请访问 /hello 或 /user";
        send_http_response(client_fd,"404 Not Found","text/plain;charset=utf-8",body);
    }

    //---------------------------------
    //send(client_fd,temp_response,strlen(temp_response),0);//这里是把信息发给谁?,这时候如果不用http连接，使用telnet访问会有什么结果？
    //关闭客户端套接字,释放资源
    close(client_fd);
    cout<<"客户端连接关闭(client_fd="<<client_fd<<")"<<endl;
    pthread_exit(NULL);
    return NULL;//这里一定要返回吗？pthread_exit不是会自动返回吗？或者只需要return NULL即可，不需要exit
};
int main(){
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    cout<<"服务监听套接字已创建,标识符为:"<<server_fd<<endl;

//1、绑定ip和端口
    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(Port);

    bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
    cout<<"端口:"<<Port<<"绑定成功"<<endl;
//2、开始监听
    listen(server_fd,max_clients);
    cout<<"http服务器启动，等待客户端连接(访问localhost:8080)"<<endl;
//3、循环接收客户端链接
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len=sizeof(client_addr);
        //accept
        int client_fd=accept(server_fd,(struct sockaddr *)&client_addr,&client_addr_len);
        cout<<"新客户端连接：ip="<<inet_ntoa(client_addr.sin_addr)<<",端口="<<ntohs(client_addr.sin_port)<<",client标识符="<<client_fd<<endl;
//4、创建子线程，让子线程处理该客户通信
        pthread_t tid;
        int * p_client=new int(client_fd);//这句什么意思
        if (pthread_create(&tid, NULL, client_handler, (void*)p_client) != 0) {
            perror("pthread_create error");
            delete p_client;
            close(client_fd);
            continue;
        }
        pthread_detach(tid);


    }
    close(server_fd);
    return 0;
}