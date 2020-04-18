#ifndef __NGX_GBLDEF_H__
#define __NGX_GBLDEF_H__

typedef struct {
    char ItemName[50];
    char ItemContent[500];
}CConfItem, *LPCConfItem;


//日志相关的结构定义
typedef struct {
    int log_level; //日志级别
    int fd;       //日志文件描述符
}ngx_log_t;

//外部全局声明

extern size_t      g_argvneedmem;
extern size_t      g_envneedmem; 
extern int         g_os_argc; 
extern char        **g_os_argv;
extern char        *gp_envmem;


extern pid_t ngx_pid;
extern pid_t       ngx_parent;
extern ngx_log_t ngx_log;


#endif