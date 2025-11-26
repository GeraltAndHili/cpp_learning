#include<iostream>
#include<string>
#include<mysql/mysql.h>
#include<openssl/sha.h>
#include<iomanip>
#include<sstream>
#include<vector>
#include<cstring>
#include<cstdio>
using namespace std;
class user_register{
    private:
        MYSQL * db1;
        bool is_connected;
    public:
        //1构造函数，负责连接数据库
        user_register(const char* host,const char * user,const char * pass,const char * db_name){//这里的参数是数据库管理层面的认证
            //初始化mysql对象
            db1=mysql_init(NULL);//这里什么含义？//这里会初始化一个mysql结构体，即数据库连接句柄
            is_connected=false;

            if(db1==NULL){
                cout<<"mysql数据库初始化失败"<<endl;
                return;
            }
            //连接数据库(默认端口3306)
            if(mysql_real_connect(db1,host,user,pass,db_name,3306,NULL,0)){
                cout<<"[系统消息]数据库连接成功,数据库实例："<<db_name<<"，用户名："<<user<<endl;
                is_connected=true;
            }else{
                cout<<"[error]连接失败"<<mysql_error(db1)<<endl;
            }
        }
        //2、析构函数,主要负责断开连接
        ~user_register(){
            if(db1!=NULL){
                mysql_close(db1);
                cout<<"[系统消息]数据库 "<<db1<<" 连接断开"<<endl;
            }
        }

        //3、核心：执行注册逻辑
        bool registerUser(string username,string password,int age){//这里的用户名和密码跟前面初始化的用户名和密码有关联吗？为啥前面用char * 这里用string
            //没有关联，不是同一个账户，前面是管理员账户，前面参数类型改成string也没问题
            //失败情况
            if(!is_connected){
                cout<<"[error]未连接到数据库"<<endl;
                return false;
            }
            //缓存sql语句
            char sql[1024];
            //构造sql语句
            snprintf(sql,sizeof(sql),"INSERT INTO users (username,password,age) VALUES ('%s','%s','%d')",username.c_str(),password.c_str(),age);//这句是什么语法？
            //这是c语言中的格式化输出语法，常用于生成日志或者构造sql语句
            
            
            //这里打印下sql语句用于debug
            cout<<"[debug]待执行："<<sql<<endl;
            //执行sql
            if(mysql_query(db1,sql)!=0){
                cout<<"[error]注册失败："<<mysql_error(db1)<<endl;
                return false;
            }
            //执行成功
       
            return true;
        }
 //4、新增了状态检查方法，连接或者初始化失败时后续操作终止
        bool isconnected()const{
            return is_connected;
        }

};
int main(){
    const char * HOST ="127.0.0.1";
    const char* USER="wjp_cpp";
    const char* PASS="wjp123456";
    const char *DB ="cpp_demo";
    //1创建对象，触发构造函数连接数据库
    user_register manager(HOST,USER,PASS,DB);

    if(!manager.isconnected()){
        cout<<"[系统消息]数据库连接失败，程序退出"<<endl;
        return 1;
    }
    //2接受用户输入
    string user_input;
    string pass_input;
    int age_input;

    cout<<"===欢迎注册==="<<endl;
    cout<<"请输入用户名"<<endl;
    cin>>user_input;

    cout<<"请输入密码"<<endl;
    cin>>pass_input;

    cout<<"请输入年龄"<<endl;
    cin>>age_input;

    //3调用1中的对象的注册功能
    bool result=manager.registerUser(user_input,pass_input,age_input);

    if(result){
        cout<<">>>用户"<<user_input<<"注册成功！"<<endl;
    }else{
        cout<<">>>注册失败！请重试"<<endl;
    }
    return 0;
    //manager对象会自动销毁，自动触发析构函数断开数据库连接
}