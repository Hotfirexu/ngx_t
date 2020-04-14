#ifndef __NGX_GBLDEF_H__
#define __NGX_GBLDEF_H__

typedef struct {
    char ItemName[50];
    char ItemContent[500];
}CConfItem, *LPCConfItem;

//外部全局声明

extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;

#endif