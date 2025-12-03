#include<unistd.h>
#include<iostream>
#include<condition_variable>
#include<memory>
#include<mutex>
#include<string>
#include<mysql/mysql.h>
#include<vector>
#include<thread>
#include<chrono>
#include<queue>
#pragma once//这里是什么意思
using namespace std;
class Mysql_Connect_Pool{
private:
    Mysql_Connect_Pool();//什么是单例模式，怎么体现的
    void Produceconnection();//生产者线程，创建链接
    void Recycleconnection();//回收线程：销毁超时链接
    MYSQL * Newmysqlconnection();//一个新的mysql连接，它和前面的生产者线程有何区别？//

    queue<pair<MYSQL*,chrono::steady_clock::time_point>> queue_connection;//这里参数什么含义//第二个参数时连接上一次被放回连接池的时间点
    mutex my_mutex;
    condition_variable cond;

    int max_size=10 ;//最大连接数
    int currentsize;//当前已创建的连接数量
    int timeout_sec=10;//空闲超时后，选择回收

    string host="127.0.0.1";
    string user="wjp_cpp";
    string password="wjp123456";
    string dbname="cpp_demo";
    int port=3306;

public:
    static Mysql_Connect_Pool * getinstance();//这里是什么作用，函数名有和特点？
    MYSQL * getconnection();//获取链接
    void returnconnection(MYSQL * conn);//归还链接
     // 禁用拷贝
    Mysql_Connect_Pool(const Mysql_Connect_Pool&) = delete;
    Mysql_Connect_Pool& operator=(const Mysql_Connect_Pool&) = delete;

};