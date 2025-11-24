#include<vector>
#include<queue>
#include<thread>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<unistd.h>
#include<iostream>
#include<atomic>
#include<chrono>
using namespace std;
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
//模拟任务申请
void testtask(int id,const string &msg){
    cout<<"任务"<<id<<"执行:"<<msg<<",线程id:"<<this_thread::get_id()<<endl;
    this_thread::sleep_for(chrono::milliseconds(100));//模拟任务耗时
}
int main(){
    thread_pool pool(4);
    for(int i=0;i<10;i++){
        pool.submit(testtask,i,"Hello threadpool");
    }
    


    return 0;
}

