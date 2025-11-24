#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>

using namespace std;
int main(int argc ,char *argv[]){//第二个参数是字符指针数组
    if(argc!=3){//这里3是什么意思？命令行参数必须为：程序名、源文件路径、目标文件路径,
        printf("用法：%s 源文件路径 目标文件路径\n",argv[0]);//一般argv[0]是本程序名（程序自身路径），argv[1]是源文件路径，argv[2]是目标文件路径
        return 1;//1什么含义?
    }
    //打开源文件
    int source_file_fd=open(argv[1],O_RDONLY);
    if(source_file_fd==-1){
        perror("open 源文件失败");
        return 1;
    }
    //打开目标文件
    int destnition_file_fd=open(argv[2],O_WRONLY|O_CREAT|O_TRUNC,0644);//0644是什么含义？其实就是设置了文件读写的权限
    if(destnition_file_fd==-1){
        perror("open 目标文件失败");
        close(source_file_fd);//关闭源文件
        return 1;
    }
    //复制文件内容
    char buff[1024];//1024字节的缓冲区  为什么要设置缓冲区？如果不设置的话，系统调用会每次只读写一个字节，效率会非常低下，有了缓冲区，一次读写1kB个字节。
    ssize_t n;//n是什么含义？ssize_t又是什么含义？----------n是存储每次read系统调用读取到的实际字节数，ssize_t是一种有符号整数类型，通常用于表示可以存储的字节数。
    //从源文件读取数据
    while((n=read(source_file_fd,buff,sizeof(buff)))>0){
        ssize_t written=0;
        while(written<n){
            ssize_t ret=write(destnition_file_fd,buff+written,n-written);//这里什么意思？原来不一定一次weire能写完所有的buff中的数据，可能要分几次才能写完
            if(ret==-1){
                perror("写入失败");
                goto end;
            }
            written+=ret;//更新已写入的字节数
        }
    }
    if(n==-1){
        perror("读取失败");
    }
end:
close(-1);
close(destnition_file_fd);
return 0;
}