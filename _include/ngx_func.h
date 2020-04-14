#ifndef __NGX_FUNC_H__
#define __NGX_FUNC_H__



void Rtrim(char *string);
void Ltrim(char *string);

void ngx_init_setproctitle();
void ngx_setproctitle(const char *title);

#endif