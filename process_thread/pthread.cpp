#include<iostream>
#include<unistd.h>
#include<pthread.h>
using namespace std;

//线程创建函数:
/*
    int pthread_create(
        pthread_t *thread,    //线程id（输出参数）
        const pthread_attr_t * attr,  //线程属性（NULL表示默认）
        void*(*start_routine)(void*), //线程执行的函数（函数指针）-----------------------------------搞清楚函数名和函数指针，特别要理解这个函数得声明到底何意味
        void *arg               //传递给线程函数的参数
    )
*/
void * thread_task(void * arg){//注意：thread_task是一个函数指针，它指向一个函数(这个函数的参数是void*类型，返回值也是void*类型),它本质是thread_task在内存中的地址
    int thread_id=*(int *)arg;
    cout<<"线程"<<thread_id<<"启动，线程id为："<<pthread_self()<<endl;
    sleep(0-thread_id+2);
    cout<<"我是线程"<<thread_id<<"，我完成了工作"<<endl;
    cout<<"线程"<<thread_id<<"结束"<<endl;
    pthread_exit(NULL); //线程退出
}
int main(){
    const int thread_num=3;
    pthread_t threadids[thread_num];
    int thread_arg[thread_num];//这两个数组分别存放线程id和传递给线程函数的参数
    //创建多个线程:
    for(int i=0;i<thread_num;i++){
        thread_arg[i]=i;
        int ret=pthread_create(&threadids[i],NULL,thread_task,&thread_arg[i]);//有个问题，我并没有直接调用thread_task，也没有传参数给它，为什么他会正常执行？
        if(ret!=0){
            cerr<<"创建线程"<<i<<"失败!错误码:"<<ret<<endl;
            return 1;//为什么要return 1？因为main函数的返回值表示程序的退出状态，0表示成功，非0表示失败
        }
    }
    //主线程等待所有子线程结束，因为一旦主线程退出，整个进程就会终止，子线程也会被强制终止
    for(int i=0;i<thread_num;i++){
        pthread_join(threadids[i],NULL);
        cout<<"主线程等待线程"<<i<<"结束"<<endl;
    }
    cout<<"所有线程完成，主线程结束"<<endl;
    return 0;
}
