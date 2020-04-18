

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>   //信号相关头文件 


#include "ngx_macro.h"   //各种宏定义
#include "ngx_func.h"
#include "ngx_c_conf.h"

//本文件用的函数声明
static void freesource();


char **g_os_argv;    //原始命令行参数数组，在main中会被赋值
char *gp_envmem = NULL;  //指向自己分配的env环境变量的内存
size_t g_argvneedmem = 0;    //环境变量所占内存大小
size_t  g_envneedmem=0;        //保存下这些argv参数所需要的内存大小
int     g_os_argc;              //参数个数 
int     g_daemonized=0;         //守护进程标记，标记是否启用了守护进程模式，0：未启用，1：启用了

pid_t ngx_pid;     //当前进程的pid
pid_t ngx_parent;  //父进程的pid
int   ngx_process;  //用来记录进程类型，master or work

sig_atomic_t  ngx_reap; //标记子进程状态变化   sig_atomic_t:系统定义的类型：访问或改变这些变量需要在计算机的一条指令内完成

int main(int argc, char *const *argv) {

    int exitcode = 0;
    int i = 0;
   
    ngx_pid = getpid();
    ngx_parent = getppid();     //取得父进程的id 
    
    

    //统计argv所占的内存
    g_argvneedmem = 0;
    for(i = 0; i < argc; i++) {  //argv =  ./nginx -a -b -c asdfas
        g_argvneedmem += strlen(argv[i]) + 1; //+1是给\0留空间。
    } 
    //统计环境变量所占的内存。注意判断方法是environ[i]是否为空作为环境变量结束标记
    for(i = 0; environ[i]; i++)  {
        g_envneedmem += strlen(environ[i]) + 1; //+1是因为末尾有\0,是占实际内存位置的，要算进来
    } //end for

    
    g_os_argc = argc;           //保存参数个数
    g_os_argv = (char **) argv; //保存参数指针

    ngx_log.fd = -1;      //-1:表示日志文件尚未打开，因为后边ngx_log_stderr要用所以这里先给-1
    ngx_process = NGX_PROCESS_MASTER;  //先标记成master
    ngx_reap = 0;          //标记子进程没发生变化
    

    CConfig *p_config = CConfig::GetInstance();
    if (p_config->Load("nginx.conf") == false) {
        ngx_log_init(); //初始化日志
        ngx_log_stderr(0,"配置文件[%s]载入失败，退出!","nginx.conf");
        exitcode = 2;
        goto lblexit;
    }

    ngx_log_init();
    if (ngx_init_signals() != 0) {
        exitcode = 1;
        goto lblexit;
    }
    
    ngx_init_setproctitle();

    if (p_config->GetIntDefault("Daemon", 0) == 1) {
        int cdaemonresult = ngx_daemon();
        if (cdaemonresult == -1) {
            exitcode = -1;
            goto lblexit;
        }
        if (cdaemonresult == 1) {
            freesource();
            exitcode = 0;
            return exitcode;
        }

        g_daemonized = 1;
    }

    ngx_master_process_cycle(); //不管父进程还是子进程，正常工作期间都在这个函数里循环；

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


//sysctl -w kernel.core_pattern=/corefile/core-%e-%p-%t
//kill -s SIGSEGV $$