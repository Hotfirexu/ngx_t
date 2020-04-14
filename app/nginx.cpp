

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "ngx_func.h"
#include "ngx_c_conf.h"


char **g_os_argv;    //原始命令行参数数组，在main中会被赋值
char *gp_envmem = NULL;  //指向自己分配的env环境变量的内存
int g_environlen = 0;    //环境变量所占内存大小

int main(int argc, char *const *argv) {

    g_os_argv = (char **)argv;
    ngx_init_setproctitle();
    ngx_setproctitle("just_test");


    printf("hello world!\n");

    printf("程序退出\n");

    CConfig *p_config = CConfig::GetInstance();
    if (p_config->Load("nginx.conf") == false) {
        printf("config file load fail!\n");
        exit(1);
    }
    printf("load success!\n");

    int port = p_config->GetIntDefault("ListenPort", 12345);
    printf("port = %d\n", port);

    for (;;) {
        sleep(1);
        printf("sleep one second.\n");
    }

    if (gp_envmem) {
        delete []gp_envmem;
        gp_envmem = NULL;
    }
    return 0;
}
