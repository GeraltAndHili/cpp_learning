#include<iostream>
#include<unistd.h>
#include<stdlib.h>
#include<sys/wait.h>
using namespace std;

int main(){
    pid_t pid=fork();
    if(pid==0){
        cout<<"子进程用exit()终止,";
        exit(0);
    }else{
        wait(NULL);
        cout<<"父进程用_exit()终止";//<<flush;
        _exit(0);
    }
    return 0;
}