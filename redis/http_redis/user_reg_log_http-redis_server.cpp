#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<vector>
#include<thread>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<atomic>
#include<chrono>
#include<cjson/cJSON.h>
#include<hiredis/hiredis.h>
#include"redis_reg_log_demo.h"
using namespace std;
//----简单线程池------
using Task =function<void()>;
class thread_pool{
private:
    vector<thread>workers;
    queue<Task>q_task;
    mutex q_mutex;
    condition_variable cond;
    bool stop;
//为什么不定义一个线程数量参数呢？之前数据库连接池就有这样一个变量
    void work_loop(){
        while(true){
            Task task;
            {
                unique_lock<mutex>lock(q_mutex);
                while(!stop&&q_task.empty()){
                    cond.wait(lock);
                }
                if(stop&&q_task.empty()){
                    return;
                }
                task=move(q_task.front());

                q_task.pop();


            }//工作循环函数为什么不用管workers数组？不应该有模块的功能调用池中线程来处理任务吗？
            task();
        }
    }
public:
    explicit thread_pool(size_t threads=5):stop(false){
        if(threads==0){
            perror("线程数量至少为1");
        }
        for(size_t i=0;i<threads;i++){
            workers.emplace_back([this]{this->work_loop();});//这里只是单纯调用了工作循环，但是并没有发现线程创建？这里向数组插入的内容具体是什么意思？
        }
        cout<<"线程池启动,线程数量:"<<threads<<endl;
    }
    ~thread_pool(){
        {
            unique_lock<mutex>lock(q_mutex);
            stop=true;
        }
        cond.notify_all();
        for(thread & t:workers){
            if(t.joinable()){
                t.join();
            }
        }
        cout<<"线程池已关闭"<<endl;

    }
    template<class T,class... Args>
    void submit(T&& t,Args &&... args){//这个submit函数是干什么的
        auto bound=bind(forward<T>(t),forward<Args>(args)...);
        {
            unique_lock<mutex> lock(q_mutex);
            if(stop){
                throw runtime_error("线程池已终止");
            }
            q_task.emplace([bound](){bound();});
        }
        cond.notify_one();
    }

};

/* http帮助函数 */
const int PORT=8080;
const int BUFF_SIZE=8192;

/* http响应函数 
  参数含义：
   - client_fd: 客户端 socket fd
   - status: HTTP 状态行，比如 "200 OK" 或 "400 Bad Request"
   - content_type: "application/json; charset=utf-8" 等
   - body: 响应体（字符串），支持空串
   */
void send_http_response(int client_fd,const char* status,const char * content_type,const string &body){//这和我之前http里的响应函数的功能区别具体在哪里
    //构造响应头并发送
    char header[512];
    int header_len=snprintf(header,sizeof(header),"HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: Close\r\n"
        "\r\n",status,content_type,body.size());//解释一下这里代码的含义
    //先发送头，在发送body
    send(client_fd,header,header_len,0);
    if(!body.empty()){
        send(client_fd,body.data(),body.size(),0);//这里body是什么
    }
}
/* 从原始请求中解析 method 和 path（只处理简单情况）：
   - request_head: 指向请求头起始（C 字符串）
   返回 pair<method, path>
*/
pair<string,string> parse_request_line(const char *request_head) {
    // 把第一行提取出来
    // 第一个空格分隔 method, 第二个空格分隔 path, 后面忽略
    char method[32] = {0}, path[256] = {0};
    sscanf(request_head, "%31s %255s", method, path); // sscanf 安全长度限制
    return {string(method), string(path)};
}

/* 从请求头（完整头）中查找 Content-Length 值（若无则返回 0） */
size_t get_content_length_from_headers(const string &headers) {
    // 查找 "Content-Length:" （不区分大小写更好，但这里假设客户端常规）
    const string key = "Content-Length:";
    auto pos = headers.find(key);
    if (pos == string::npos) {
        // 尝试小写形式（更宽容）
        auto pos2 = headers.find("content-length:");
        if (pos2 == string::npos) return 0;
        pos = pos2;
    }
    // 从冒号后面读整数
    pos += key.size();
    // 跳过空白
    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t')) ++pos;
    size_t end = pos;
    while (end < headers.size() && isdigit((unsigned char)headers[end])) ++end;
    if (end > pos) {
        return (size_t)stoul(headers.substr(pos, end-pos));
    }
    return 0;
}

/* 检查 Content-Type 是否 application/json（非严格，仅包含判断） */
bool is_content_type_json(const string &headers) {
    auto pos = headers.find("Content-Type:");
    if (pos == string::npos) pos = headers.find("content-type:");
    if (pos == string::npos) return false;
    // 取从 pos 到下一行的子串
    size_t line_end = headers.find("\r\n", pos);
    if (line_end == string::npos) line_end = headers.size();
    string ct = headers.substr(pos, line_end - pos);
    // 转小写以便比较
    for (auto &c: ct) c = tolower((unsigned char)c);
    return ct.find("application/json") != string::npos;
}//从111行到这里，我都不太理解，但是是不是不影响我掌握http？或者你后面给我讲一下http传输的信息的必须掌握的一些关键字和格式
extern bool register_user(const string &username, const string &password, redisContext *rconn);
extern bool login_user(const string &username, const string &password, redisContext *rconn);//这里是之前现成的数据库登录和注册模块
/* http处理 POST /register
   - body: POST 请求体字符串（期望 JSON: {"username":"...","password":"..."}）
   - rconn: redis 连接指针（传给上面的            register_user                ）
*/
void handle_register(int client_fd,const string &body,redisContext *rconn){
    if(body.empty()){
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"empty body"})");
        return;
    }
    cJSON *json=cJSON_Parse(body.c_str());
    if(!json){
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"invalid json"})");
        return;
    }
    cJSON *juser=cJSON_GetObjectItem(json,"username");
    cJSON *jpass=cJSON_GetObjectItem(json,"password");
    string username = (juser && cJSON_IsString(juser)) ? juser->valuestring : "";
    string password = (jpass && cJSON_IsString(jpass)) ? jpass->valuestring : "";
    cJSON_Delete(json);
    if (username.empty() || password.empty()) {
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"username and password required"})");
        return;
    }
    //调用register_user模块
    bool ok =register_user(username,password,rconn);
    if (!ok) {
        // 这里根据 register_user 的返回值判断可能是用户名冲突或内部错误，真实场景可以更细化返回码
        send_http_response(client_fd, "409 Conflict", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"registration failed or user exists"})");
    } else {
        send_http_response(client_fd, "200 OK", "application/json; charset=utf-8",
                           R"({"ok":true,"message":"registered"})");
    }
}
/* http处理post/login 成功返回200且body的ok为true，不成功返回401 */
void handle_login(int client_fd,const string &body,redisContext * rconn){
    if(body.empty()){
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"empty body"})");
        return;
    }
    cJSON * json=cJSON_Parse(body.c_str());//为什么要转换成c_str？包括之前用字符串形式写的sql语句去mysql中查询也要使用c_str()
    if(!json){
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"invalid json"})");
        return;       
    }
    cJSON *juser = cJSON_GetObjectItem(json, "username");
    cJSON *jpass = cJSON_GetObjectItem(json, "password");
    string username = (juser && cJSON_IsString(juser)) ? juser->valuestring : "";
    string password = (jpass && cJSON_IsString(jpass)) ? jpass->valuestring : "";
    cJSON_Delete(json);

    if (username.empty() || password.empty()) {
        send_http_response(client_fd, "400 Bad Request", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"username and password required"})");
        return;
    }

    bool ok = login_user(username, password, rconn);
    if (!ok) {
        send_http_response(client_fd, "401 Unauthorized", "application/json; charset=utf-8",
                           R"({"ok":false,"error":"invalid credentials"})");
    } else {
        // 登录成功：真实系统此处应该发放 session/token；这里先返回成功标记
        send_http_response(client_fd, "200 OK", "application/json; charset=utf-8",
                           R"({"ok":true,"message":"login success"})");
    }
}
/* http客户端连接处理:解析请求头与body，分派到各接口 */
void handle_client_socket(int client_fd,redisContext * rconn){
    //读取请求头
    string raw;
    raw.reserve(4096);//什么概念
    //先读取直到双crlf
    char buf[BUFF_SIZE];
    ssize_t n;
    bool header_done=false;
    while(!header_done){
        n=recv(client_fd,buf,sizeof(buf),0);
        if(n<=0){
            close(client_fd);
            return;
        }
        raw.append(buf,buf+n);
        if(raw.find("\r\n\r\n")!=string::npos)
            header_done=true;
        //这里做一个简单保护
        if (raw.size() > 64*1024) { // 64KB 限制
            send_http_response(client_fd, "413 Payload Too Large", "text/plain; charset=utf-8", "too large");
            close(client_fd);
            return;
        }
    }
    //分割头和体
    size_t header_end=raw.find("\r\n\r\n");
    string header_str=raw.substr(0,header_end+4);//+4是为了包含\r\n\r\n
    string body="";
    //如果已经recv时读到了一部分body，也要存下来
    if(raw.size()>header_end+4){
        body=raw.substr(header_end+4);
    }
    //解析头（method和path
    auto request_bound_pos=header_str.find("\r\n");
    string request=header_str.substr(0,request_bound_pos);
    auto method_pos=parse_request_line(request.c_str());
    string method=method_pos.first;//这里的method_pos是什么结构？
    string path=method_pos.second;
    // 如果是 POST，需要根据 Content-Length 读取剩余 body
    size_t content_len = get_content_length_from_headers(header_str);
    if (method == "POST") {//post和get都什么区别，具体是什么东西
        // 如果 body 已经比 Content-Length 短，则继续 recv 直到完整
        while (body.size() < content_len) {
            n = recv(client_fd, buf, sizeof(buf), 0);
            if (n <= 0) { close(client_fd); return; }
            body.append(buf, buf + n);
            if (body.size() > 10*1024*1024) { // 10MB 限制防滥用
                send_http_response(client_fd, "413 Payload Too Large", "text/plain; charset=utf-8", "too large");
                close(client_fd);
                return;
            }
        }
        // 截断到 content_len（多余的部分不处理）
        if (body.size() > content_len) body.resize(content_len);
    }

    // 现在根据 path 分派到不同逻辑
    if (method == "GET" && path == "/hello") {
        send_http_response(client_fd, "200 OK", "text/plain; charset=utf-8", "你好，这里是http服务器");
    } else if (method == "GET" && path == "/user") {
        // 简单示例返回 JSON
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "username", "示例用户");
        cJSON_AddNumberToObject(root, "age", 21);
        char *json_body = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        string s = json_body ? json_body : "";
        free(json_body);
        send_http_response(client_fd, "200 OK", "application/json; charset=utf-8", s);
    }
    else if (method == "POST" && path == "/register") {
        // 可选：验证 Content-Type 是 JSON
        if (!is_content_type_json(header_str)) {
            send_http_response(client_fd, "415 Unsupported Media Type", "application/json; charset=utf-8",
                               R"({"ok":false,"error":"content-type must be application/json"})");
        } else {
            handle_register(client_fd, body, rconn);
        }
    }
    else if (method == "POST" && path == "/login") {
        if (!is_content_type_json(header_str)) {
            send_http_response(client_fd, "415 Unsupported Media Type", "application/json; charset=utf-8",
                               R"({"ok":false,"error":"content-type must be application/json"})");
        } else {
            handle_login(client_fd, body, rconn);
        }
    }
    else {
        send_http_response(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
    }

    // 关闭连接（Connection: Close）
    close(client_fd);
}
/* main 启动server并把redis连接传给worker */
int main(){
    //初始化redis
    redisContext * rconn=redisConnect("127.0.0.1",6379);//redis也需要ip地址吗？
    if(!rconn||rconn->err){
        if(rconn){
            cerr<<"redis连接错误"<<rconn->errstr<<endl;
            redisFree(rconn);
            rconn=nullptr;//为什么还要再赋值为空，不是已经释放了吗
        }else{
            cerr<<"redis 连接失败:"<<"无redis可用"<<endl;
        }
    }else{
        cout<<"redis连接成功"<<endl;
    }
    //启动socket
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        perror("socket无法创建");
        return 1;
    }
    int opt=1;//opt为啥等于1
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(server_fd,(struct sockaddr*)&addr,sizeof(addr))==-1){
        perror("socket绑定地址失败");
        close(server_fd);
        return 1;
    }
    if(listen(server_fd,128)==-1){
        perror("服务器无法监听");
        close(server_fd);
        return 1;
    }
    cout<<"HTTP 服务器正常并启动监听,端口为:"<<PORT<<endl;
    //线程池启动
    thread_pool pool(8);
    //http循环接受请求（借助线程池来处理http所接收到的任务）
    while(true){
        struct sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);
        int client_fd=accept(server_fd,(struct sockaddr *)&client_addr,&client_len);
        if(client_fd==-1){
            perror("接收信息失败");
            close(client_fd);//这里需要主动关闭吗？
            continue;
        }
        cout<<"客户端已连接,地址为:"<<inet_ntoa(client_addr.sin_addr)<<"端口为:"<<ntohs(client_addr.sin_port)<<"客户端标识符为:"<<client_fd<<endl;
        //接下来把链接交给线程池
        pool.submit([client_fd,rconn](){
            handle_client_socket(client_fd,rconn);
        });
    }
    //程序异常才会到这里
    if(rconn)
        redisFree(rconn);
    close(server_fd);
    return 0;
    
}
