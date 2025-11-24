#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<sys/socket.h>
#include<cjson/cJSON.h>
#include<vector>
#include<queue>
#include<thread>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<chrono>

using namespace std;
//线程池
using Task=function<void()>;
class thread_pool{
    private:
        vector<thread>workers;//这里的thread是怎么样的一个类型？
        queue<Task> tasks;
        mutex queue_mutex;
        condition_variable cond;
        bool stop;
//线程池的工作循环
        void worker_loop(){
            while(true){
                Task task;
                {//临界区开始:获取锁//使用lambda的优点在哪里？
                    unique_lock<mutex>lock(queue_mutex);//这一句什么含义   //新建锁对象并上锁  //它在作用于结束时会自动解锁
                    while(!this->stop&&this->tasks.empty()){//没任务时先等待
                        cond.wait(lock);
                    }
                    if(this->stop&&this->tasks.empty()){
                        return;//信号停止并且任务也空
                    }

                    task=move(this->tasks.front());//取出任务
                    this->tasks.pop();
                }
                task();//为什么task又能被赋值又能直接调用？//这里其实就是把刚才从队列里取出的任务完成
            }
        }
         
        
    public:
    //构造函数
        explicit thread_pool(size_t threads):stop(false){
            if(threads==0){
                perror("线程数量至少大于1");
            }
            for(size_t i=0;i<threads;i++){
                workers.emplace_back([this]{this->worker_loop();});//这一句是什么语法？
            };
            cout<<"线程池启动，线程容量为:"<<threads<<endl;
        }
        //析构函数
        ~thread_pool(){
            {
                unique_lock<mutex> lock(queue_mutex);//这里其实是为了释放锁，这个定义是获得queue_mutex，然后lock本身会在作用域结束后自行销毁，达到了释放锁的目的
                stop=true;
            }
            cond.notify_all();
            for(thread&worker:workers){
                if(worker.joinable()){//这里什么含义
                    worker.join();//什么含义    
                }
            };
            cout<<"线城池成功关闭"<<endl;
        }
        //获取任务并提交给内部的线程池
        template<class T,class... Args>
        void submit(T && t,Args&&... args){//提交任务给线程池
            auto task=bind(forward<T>(t),forward<Args>(args)...);//task这里是一个对象，本身是一个可调用对象的实例，保存了要执行的函数和绑定的参数（通过bind绑定）

            unique_lock<mutex>lock(queue_mutex);
            if(stop){
                throw runtime_error("线程池已停止，无法提交");
            }

            tasks.emplace([task](){
                task();//task()是对这个可调用对象（task）的调用，等价于执行task这个实例保存的任务。实际上是执行了operator()。
            }
            );//这段整体含义就是把一个lambda表达式存入了tasks队列，随后被唤醒的工作线程从队列取出lambda并执行，而这个lambda就调用task()

            //那么为什么要搞得这么复杂，用lambda捕获task呢？因为task是submit的局部变量，submit函数结束后task就被销毁了。这里的捕获实际是把任务重新封装了
            //一遍，保证能正常传递给工作线程



            cond.notify_one();
        }

};

//全局变量定义
const int Port=8888;
const int Buff_size=4096;
const int max_clients=128;
//工具函数：去除字符串两端的空白字符（空格、制表符、\r、\n）
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
    if(!body)
        body="";
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
void * client_handler(void * argv){
    int client_fd=*(int *)argv;
    delete(int *)argv;//这里为什么要手动删除，为什么要强转？
    //接受客户端的http请求
    char recv_buff[Buff_size]={0};
    ssize_t recv_len=recv(client_fd,recv_buff,sizeof(recv_buff)-1,0);
     if(recv_len <= 0){ // recv返回-1（错误）或0（客户端断开）
        cout<<"recv失败或客户端断开，client_fd="<<client_fd<<endl;
        close(client_fd);
        pthread_exit(NULL);
    }
    //打印请求
    cout<<"收到http请求：\n"<<recv_buff<<endl;
    //解析请求
    char method[64]={0};//请求方法
    char path[256]={0};//请求路径

    sscanf(recv_buff,"%s %s %*s",method,path);//这里什么意思，为什么要用sscanf？不能直接赋值吗//sscanf 的作用：按指定格式从字符串中提取数据（类似 “字符串版的 scanf”）
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
    }// 新增：处理GET /index.html接口（返回静态HTML文件）
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
    }
    else{
        const char * body="404 Not Found!请访问 /hello 或 /user";
        send_http_response(client_fd,"404 Not Found","text/plain;charset=utf-8",body);
    }

    //---------------------------------
    //send(client_fd,temp_response,strlen(temp_response),0);//这里是把信息发给谁?,这时候如果不用http连接，使用telnet访问会有什么结果？
    //关闭客户端套接字,释放资源
    close(client_fd);
    cout<<"客户端连接关闭(client_fd="<<client_fd<<")"<<endl;
    pthread_exit(NULL);
    //return NULL;//这里一定要返回吗？pthread_exit不是会自动返回吗？或者只需要return NULL即可，不需要exit//这一句确实是多余的
};
int main(){
    int opt=1;
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&opt,sizeof(opt));
    if(server_fd == -1){
        perror("socket创建失败");
        return -1;
    }
    cout<<"服务监听套接字已创建,标识符为:"<<server_fd<<endl;

//1、绑定ip和端口
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));

    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(Port);

    if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1){
        perror("端口绑定失败（可能端口已被占用）");
        close(server_fd);
        return -1;
    }
    cout<<"端口:"<<Port<<"绑定成功"<<endl;
//2、开始监听
    if(listen(server_fd,max_clients) == -1){
        perror("监听失败");
        close(server_fd);
        return -1;
    }
    cout<<"http服务器启动，等待客户端连接(访问localhost:8888)"<<endl;

    //调用线程池
    thread_pool pool(10);

//3、循环接收客户端链接
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len=sizeof(client_addr);
        //accept
        int client_fd=accept(server_fd,(struct sockaddr *)&client_addr,&client_addr_len);
        cout<<"新客户端连接：ip="<<inet_ntoa(client_addr.sin_addr)<<",端口="<<ntohs(client_addr.sin_port)<<",client标识符="<<client_fd<<endl;
//4、创建子线程，让子线程处理该客户通信
        //pool.submit(client_handler,&client_fd);-----------------------错误
        //         为什么必须用 new int？
        // 因为：
     // client_fd 是一个栈变量
     // submit() 是立即返回的
     // 若直接传 &client_fd → 下一次 accept 会覆盖 client_fd
     // 工作线程取到时客户端 fd 已被覆盖 → 错误 → 崩溃 / 阻塞    
     //修改如下
        int * pclient=new int(client_fd);
        pool.submit(client_handler,pclient);


    }
    close(server_fd);
    return 0;
}
