#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ngx_global.h"


//设置可执行程序相关函数：分噢诶内存，并且把环境变量拷贝到新内存中来
void ngx_init_setproctitle() {
    //统计环境变量所占的内存。是以environ[i]是否为空作为环境变量结束标记

    gp_envmem = new char(g_envneedmem);
    memset(gp_envmem, 0, g_envneedmem);

    char *ptmp = gp_envmem;

    //把原来的内存搬到新地方来
    for (int i= 0; environ[i]; i++) {
        size_t size = strlen(environ[i] + 1);  //结尾有一个\0
        strcpy(ptmp, environ[i]);  //把原环境变量内容拷贝到新地方【新内存】
        environ[i] = ptmp;         //然后还要让新环境变量指向这段新内存
        ptmp += size;
    }
    return;

}

void ngx_setproctitle(const char *title) {
    size_t titlelen = strlen(title);

    size_t e_environlen = 0;
    for (int i = 0; g_os_argv[i]; i++) {     //计算总的原始的argv那块内存的总长度
        e_environlen += strlen(g_os_argv[i] + 1);
    }

    size_t esy = g_argvneedmem + g_envneedmem;
    if (esy <= titlelen) { //标题长度太大了，没有足够的空间了
        return;
    }

    //设置后续的命令行参数为空，表示只有argv[]中只有一个元素了
    g_os_argv[1] = NULL;

    char *ptmp = g_os_argv[0];
    strcpy(ptmp, title);
    ptmp += titlelen;

    size_t cha = esy - titlelen;
    memset(ptmp, 0, cha);
    return;
}