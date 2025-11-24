#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include<sys/time.h>
using namespace std;

const long long max_num=100000000;
long long sigle_thread_sum(){
    long long sum =0;
    for(long long i=0;i<=max_num;i++){
        sum+=i;
    }
    return sum;
}
long long get_current_time(){
    struct  timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000+ tv.tv_usec/1000;
    
}
int main(){
    long long sum1;
    long long start_time,end_time;
    start_time=get_current_time();
    sum1=sigle_thread_sum();
    end_time=get_current_time();
    cout<<"==================单线程的结果================="<<endl;
    cout<<"1~"<<max_num<<"的和为："<<sum1<<endl;
    cout<<"单线程计算时间为:"<<end_time-start_time<<"毫秒"<<endl;
    return 0;
}