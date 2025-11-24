#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("用法：%s 源文件路径 目标文件路径\n", argv[0]);
        return 1;
    }

    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd == -1) {
        perror("open 源文件失败");
        return 1;
    }

    int dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1) {
        perror("open 目标文件失败");
        close(src_fd);
        return 1;
    }

    char buf[1024]; // 缓冲区大小 1024 字节
    ssize_t n = read(src_fd, buf, sizeof(buf));
    if (n > 0) {
        // 故意制造段错误：访问 buf[2000]（超出 1024 范围，非法内存）
        buf[2000] = 'a'; // 这一行会触发 Segmentation fault！
        write(dest_fd, buf, n);
    }

    close(src_fd);
    close(dest_fd);
    return 0;
}