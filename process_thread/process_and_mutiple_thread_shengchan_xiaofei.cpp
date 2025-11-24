#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include<queue>
using namespace std;

//共享资源：
const int max_queue_size=5;
queue<int>data_q;
pthread_mutex_t mutex;
pthread_cond_t queue_not_empty;
pthread_cond_t queue_not_full;
bool produce_done=false;//生产者是否完成生产


//生产者线程函数
void * producer(void * arg){
    int producer_id=*(int *)arg;
    cout<<"[生产者"<<producer_id<<"]启动,开始生产数据(共10个生产者)……"<<endl;
    for(int i=0;i<10;i++){
        pthread_mutex_lock(&mutex);

        while(data_q.size()>=max_queue_size){
            cout<<"[生产者"<<producer_id<<"]队列已满(容量"<<max_queue_size<<"),等待消费"<<endl;
            pthread_cond_wait(&queue_not_full,&mutex);//这个函数什么作用?
        }

        data_q.push(i);
        cout<<"[生产者"<<producer_id<<"]生产数据："<<i<<",当前队列大小："<<data_q.size()<<endl;

        pthread_cond_signal(&queue_not_empty);

        pthread_mutex_unlock(&mutex);

        sleep(2);
    }
    pthread_mutex_lock(&mutex);//为什么这里要上锁？
    produce_done=true;
    pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&queue_not_empty);

    cout<<"[生产者"<<producer_id<<"]生产完成，退出！"<<endl;
    pthread_exit(NULL);
}
//消费者线程函数
void * consumer(void* arg){
    int consumer_id=*(int *)arg;
    cout<<"[消费者"<<consumer_id<<"]启动，等待消费数据……"<<endl;
    while(true){//为什么这里要用while(true)？因为消费者不知道什么时候生产者会生产数据，它需要一直等待直到生产者完成生产并且队列为空
        pthread_mutex_lock(&mutex);

        while(data_q.empty()&&produce_done==false){
            cout<<"[消费者"<<consumer_id<<"]队列空，等待生产者生产"<<endl;
            pthread_cond_wait(&queue_not_empty,&mutex);
        }
        if(data_q.empty()&&produce_done){
            pthread_mutex_unlock(&mutex);
            cout<<"[消费者"<<consumer_id<<"]无更多数据，退出"<<endl;
            pthread_exit(NULL);
        }
        int data=data_q.front();
        data_q.pop();
        cout<<"[消费者"<<consumer_id<<"]消费数据："<<data<<",当前队列大小："<<data_q.size()<<endl;

        pthread_cond_signal(&queue_not_full);

        pthread_mutex_unlock(&mutex);

        sleep(2);

    }
}
int main(){

pthread_mutex_init(&mutex,NULL);
pthread_cond_init(&queue_not_empty,NULL);
pthread_cond_init(&queue_not_full,NULL);

pthread_t produce_thread;
pthread_t consume_thread1,consume_thread2;
int produce_id=1;
int consume_id1=1,consume_id2=2;

pthread_create(&produce_thread,NULL,producer,&produce_id);
pthread_create(&consume_thread1,NULL,consumer,&consume_id1);
pthread_create(&consume_thread2,NULL,consumer,&consume_id2);

pthread_join(produce_thread,NULL);
pthread_join(consume_thread1,NULL);
pthread_join(consume_thread2,NULL);

pthread_mutex_destroy(&mutex);//不会自动释放吗？
pthread_cond_destroy(&queue_not_empty);
pthread_cond_destroy(&queue_not_full);
cout<<"所有线程结束，程序退出"<<endl;
return 0;
    
}
