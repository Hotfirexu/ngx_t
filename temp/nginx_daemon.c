/*************************************************************************
> Author       : doc.xu
> Mail         : jianx_xu@163.com
> File Name    : nginx_daemon.c
> Created Time : 2020-04-12 20:47:12
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

int ngx_daemon() {
    int fd;
    switch (fork())
    {
    case -1:
        /* code */
        return -1;
    case 0:// 子进程走到这里，直接break；
        break;
    default: //父进程走到这里，直接退出
        exit(0);
    }

    
    //子进程走早这里
    if (setsid() == -1) { //脱离终端，终端关闭，将跟此子进程无关
        return -1;
    }
    umask(0); //设置为0，不要让它来限制文件权限，以免引起混乱

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        return -1;
    }

    if (dup2(fd, STDIN_FILENO) == -1) {
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        return -1;
    }

    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            return -1;
        }
    }

    return 1;


}

int main(int argc, char * const *argv) {
    if (ngx_daemon() != 1) {
        return -1;
    } else {
        for(;;) {
            sleep(1);
        }
    }
    return 0;
}
