#include<iostream>
#include<unistd.h>
#include<sys/wait.h>
using namespace std;

int main(){
    cout<<"父进程启动，pid："<<getpid()<<endl;

    pid_t pid=fork();//子进程部署从main()开始重新执行，二十从fork()之后的代码开始执行
    if(pid==-1){
        perror("fork没成功");
        return 1;
    }else if(pid==0){
        cout<<"[子进程]我是子进程,pid:"<<getpid()<<",父进程 pid："<<getppid()<<endl;
        sleep(2);
        cout<<"[子进程]任务完成，退出！"<<endl;
    }else{
        cout<<"[父进程]我是父进程，pid:"<<getpid()<<",创建的子进程 pid:"<<pid<<endl;
        wait(NULL);
        cout<<"[父进程]子进程已退出，父进程结束!"<<endl;
    }
return 0;

}