#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include<sys/time.h>
using namespace std;

const long long max_num=100000000;
const int thread_count=6;

long long total_sum=0;
pthread_mutex_t mutex;


long long sigle_thread_sum(){
    long long sum =0;
    for(long long i=0;i<=max_num;i++){
        sum+=i;
    }
    return sum;
}
struct threadparam{
    long long start;
    long long end;
    int thread_id;
};

void * thread_work(void * arg){
    threadparam * param=(threadparam *)arg;
    long long local_sum=0;
    for(long long i=param->start;i<=param->end;i++){
        local_sum+=i;
    }
    pthread_mutex_lock(&mutex);
    total_sum+=local_sum;
    pthread_mutex_unlock(&mutex);

    cout<<"线程"<<param->thread_id<<"完成:负责区间["<<param->start<<"~"<<param->end<<"]的计算,局部和为:"<<local_sum<<endl;
    pthread_exit(NULL);
}


//多线程求和核心逻辑
long long muti_thread_sum(){
    pthread_t threads[thread_count];
    threadparam params[thread_count];
    long long length=max_num/thread_count;//每个线程负责的区间的长度

    pthread_mutex_init(&mutex,NULL);
    total_sum=0;

    for(int i=0;i<thread_count;i++){
        params[i].start=i*length+1;
        params[i].end=(i==thread_count-1)?max_num:(i+1)*length;
        params[i].thread_id=i;
        pthread_create(&threads[i],NULL,thread_work,&params[i]);
    }

    for(int i=0;i<thread_count;i++){
        pthread_join(threads[i],NULL);
    }

    pthread_mutex_destroy(&mutex);
    return total_sum;
}
long long get_current_time(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000+ tv.tv_usec/1000;
}
int main(){
    long long sum2;
    long long start_time,end_time;

    start_time=get_current_time();
    sum2=muti_thread_sum();
    end_time=get_current_time();

    cout<<"========多线程结果========:\n";
    cout<<"1~"<<max_num<<"的和为:"<<sum2<<endl;
    cout<<"线程数为:"<<thread_count<<endl;
    cout<<"多线程计算时间:"<<end_time-start_time<<"毫秒"<<endl;

    long long sum1;
    start_time=get_current_time();
    sum1=sigle_thread_sum();
    end_time=get_current_time();
    cout<<"==================单线程的结果================="<<endl;
    cout<<"1~"<<max_num<<"的和为："<<sum1<<endl;
    cout<<"单线程计算时间为:"<<end_time-start_time<<"毫秒"<<endl;
    return 0;

}