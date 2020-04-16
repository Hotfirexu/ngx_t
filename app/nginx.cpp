

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "ngx_func.h"
#include "ngx_c_conf.h"

//本文件用的函数声明
static void freesource();


char **g_os_argv;    //原始命令行参数数组，在main中会被赋值
char *gp_envmem = NULL;  //指向自己分配的env环境变量的内存
int g_environlen = 0;    //环境变量所占内存大小

pid_t ngx_pid;   //当前进程的pid

int main(int argc, char *const *argv) {

    int exitcode = 0;
   
    ngx_pid = getpid();
    g_os_argv = (char **)argv;
    // 
    // ngx_setproctitle("just_test");


    

    CConfig *p_config = CConfig::GetInstance();
    if (p_config->Load("nginx.conf") == false) {
        exitcode = 2;
        goto lblexit;
    }

    ngx_log_init();
    
    ngx_init_setproctitle();

    for (;;) {
        sleep(1);
        printf("sleep one second.\n");
    }

lblexit:
    freesource();
    printf("程序退出！\n");
    return exitcode;
}

void freesource() {
    if (gp_envmem) {
        delete []gp_envmem;
        gp_envmem = NULL;
    }


    if (ngx_log.fd !=  STDERR_FILENO && ngx_log.fd != -1) {
        close(ngx_log.fd);
        ngx_log.fd = -1;
    }
}
