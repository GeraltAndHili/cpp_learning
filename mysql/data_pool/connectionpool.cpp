#include "ConnectionPool.h"

Mysql_Connect_Pool * Mysql_Connect_Pool::getinstance(){
    static Mysql_Connect_Pool pool;//这样定义有什么作用？之前头文件里得私有构造函数又有什么用？
    return &pool;
}
//构造函数
Mysql_Connect_Pool::Mysql_Connect_Pool(){
    //创建最小连接数量
    for(int i=0;i<2;i++){
        MYSQL * conn=Newmysqlconnection();
        queue_connection.push({conn,chrono::steady_clock::now()});
        currentsize++;
    }


    //生产者线程，不足时创建链接
    thread producer(&Mysql_Connect_Pool::Produceconnection,this);//这里什么含义,为什么生产函数不带（）//这里是启动生产者线程
    producer.detach();//为什么在这里就dtach
    //回收线程：释放长期空闲的线程//启动回收线程
    thread recycler(&Mysql_Connect_Pool::Recycleconnection,this);//跟上面一样的问题
    recycler.detach();//问题同上
    
}
//---------------------
//创建一个真正的mysql连接
//---------------------
MYSQL * Mysql_Connect_Pool::Newmysqlconnection(){//这里可以理解成一个生产车间，而生产者函数可以理解成生产线，此函数（车间）被生产者不停调用
    MYSQL * conn =mysql_init(nullptr);

    if(!mysql_real_connect(conn,host.c_str(),user.c_str(),password.c_str(),dbname.c_str(),port,nullptr,0)){
        cout<<"mysql连接错误："<<mysql_error(conn)<<endl;
        return nullptr;
    }
    cout<<"创建新连接成功,当前连接数="<<currentsize+1<<endl;
    return conn;
}

//生产者线程:
void Mysql_Connect_Pool::Produceconnection(){
    while (true)
    {
        unique_lock<mutex>lock (my_mutex);
        /* code */
        //在没有空闲连接并且未达到最大连接数时，创建新连接
        if(queue_connection.empty()&&currentsize<max_size){
            MYSQL * conn=Newmysqlconnection();
            queue_connection.push({conn,chrono::steady_clock::now()});//这里chrono是干嘛的
            currentsize++;
            cond.notify_all();//这里唤醒的是谁？唤醒所有在该 condition_variable 上等待（cond.wait(lock)) 的线程。通常是那些在 getconnection() 中由于池空而被阻塞的消费者线程
        }
        lock.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));//这里什么作用
    }
}
//回收资源线程
void Mysql_Connect_Pool::Recycleconnection(){
    while(true){
        this_thread::sleep_for(chrono::seconds(3));//干啥的

        unique_lock<mutex>lock (my_mutex);//注意这里的锁会在作用域结束后自动解锁
        if(!queue_connection.empty()){
            auto&front=queue_connection.front();
            MYSQL * conn=front.first;//这是取什么
            auto lastusetime=front.second;//这是取什么

            auto now=chrono::steady_clock::now();

            //发现超时
            if(chrono::duration_cast<chrono::seconds>(now-lastusetime).count()>timeout_sec){
                cout<<"回收一个超时链接"<<endl;
                mysql_close(conn);
                queue_connection.pop();
                currentsize--;
            }
        }
    }
}

//消费者接口:访问并获得链接
MYSQL * Mysql_Connect_Pool::getconnection(){
    unique_lock<mutex>lock (my_mutex);
    //如果空队列
    while(queue_connection.empty()){
        cout<<"等待可用链接...."<<endl;
        cond.wait(lock);
    }
    auto item=queue_connection.front();
    queue_connection.pop();

    MYSQL * conn=item.first;
    return conn;
}
//返回链接到池中
void Mysql_Connect_Pool::returnconnection(MYSQL * conn){
    unique_lock<mutex>lock (my_mutex);
    queue_connection.push({conn,chrono::steady_clock::now()});
    cond.notify_all();//唤醒等待连接的线程;
}


int main(){
    Mysql_Connect_Pool *pool_test=Mysql_Connect_Pool::getinstance();

    MYSQL * conn=pool_test->getconnection();
    if(!conn){
        perror("未能从连接池获得链接");
        return -1;
    }
    const char * sql="select username from users where age=1";
    if(mysql_query(conn,sql)){//返回0代表成功，非0代表发生错误
        cerr<<"查询失败:"<<mysql_error(conn)<<endl;
        pool_test->returnconnection(conn);
        return -1;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row) {
            std::cout << "Query result" << row[0] << std::endl;
        }
        mysql_free_result(res);
    } else {
        std::cout << "查询无返回结果或出错" << std::endl;
    }

    //归还到到连接池
    pool_test->returnconnection(conn);
    cout<<"主线程休眠5秒以便观察日志..."<<endl;
    this_thread::sleep_for(chrono::seconds(5));
    return 0;

}