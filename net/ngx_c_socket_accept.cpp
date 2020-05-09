#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>    //uintptr_t
#include <stdarg.h>    //va_start....
#include <unistd.h>    //STDERR_FILENO等
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <fcntl.h>     //open
#include <errno.h>     //errno
//#include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"


//建立新连接用的，当新连接进入时，该函数会被ngx_epoll_process_event（）调用
void CSocekt::ngx_event_accept(lpngx_connection_t oldc) {

    struct sockaddr     mysockaddr;         //远端服务器的socket地址
    socklen_t           socklen;
    int                 err;
    int                 level;
    int                 s;
    static int          use_accept4 = 1;    //能够使用accept4函数
    lpngx_connection_t  newc;

    socklen = sizeof(mysockaddr);

    do {
        if (use_accept4) {
            /*
                listen套接字是非阻塞的，所以即使已完成连接队列为空，accept4也不会卡在这里
                从内核获取一个用户端连接，最后一个参数SOCK_NONBLOCK表示返回一个非阻塞的socket，节省一次ioctl【设置为非阻塞】调用
            */
            s = accept4(oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK);
        } else {
            s = accept(oldc->fd, &mysockaddr, &socklen);
        }

        //惊群，有时候不一定全部惊动所有的worker进程，可能只惊动其中的2个，其中一个成功，其实的accept都会返回-1；错误 (11: Resource temporarily unavailable【资源暂时不可用】) 

        if (s == -1) {

            err = errno;

            //对accept、send和recv而言，事件未发生时errno通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）
            if (err == EAGAIN) {  //accept()没准备好，这个EAGAIN错误EWOULDBLOCK是一样的
                return;

            }

            level = NGX_LOG_ALERT;
            if (err == ECONNABORTED) {//ECONNRESET错误则发生在对方意外关闭套接字后【您的主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
                
                level = NGX_LOG_ERR;

            } else if (err == EMFILE || err == ENFILE) {    //EMFILE:进程的fd已用尽【已达到系统所允许单一进程所能打开的文件/套接字总数】
                level = NGX_LOG_CRIT;
            }

            ngx_log_error_core(level,errno,"CSocekt::ngx_event_accept()中accept4()失败!");

            if (use_accept4 && err == ENOSYS) {     //accept4()函数没实现
                use_accept4 = 0;                    //标记不使用accept4()函数，改用accept()函数
                continue;                           //回去重新用accept()函数搞
            }

            return;
        }

        newc = ngx_get_connection(s);               //这里针对的是新连接的套接字
        if (newc == NULL) {

            //连接池中连接不够用，那么就得把这个socekt直接关闭并返回了，因为在ngx_get_connection()中已经写日志了，所以这里不需要写日志了
            if (close(s) == -1) {
                ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_event_accept()中close(%d)失败!",s); 
            }
            return;
        }

         //成功的拿到了连接池中的一个连接
         memcpy(&newc->s_sockaddr, &mysockaddr, socklen);   //拷贝客户端地址到连接对象【要转成字符串ip地址参考函数ngx_sock_ntop()】


         if (!use_accept4) {

             //如果不是用accept4()取得的socket，那么就要设置为非阻塞【因为用accept4()的已经被accept4()设置为非阻塞了】
             if (setnonblocking(s) == false) {
                 ngx_close_accepted_connection(newc); //回收连接池中的连接（千万不能忘记），并关闭socket
                return;
             }
         }

         newc->listening = oldc->listening;             //连接对象 和监听对象关联，方便通过连接对象找监听对象【关联到监听端口】
         newc->w_ready   = 1;                           //标记可以写，新连接写事件肯定是ready的；【从连接池拿出一个连接时这个连接的所有成员都是0】  

         newc->rhandler = &CSocekt::ngx_wait_request_handler;   //设置数据来时的读处理函数

         if (ngx_epoll_add_event(s,
                                1, 0,
                                EPOLLET,
                                EPOLL_CTL_ADD,
                                newc) == -1) {

                ngx_close_accepted_connection(newc);
                return;
        }

        break;



    }while(1);
}

void CSocekt::ngx_close_accepted_connection(lpngx_connection_t c) {
    int fd = c->fd;
    ngx_free_connection(c);

    c->fd = -1;
    if (close(fd) == -1) {
         ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_close_accepted_connection()中close(%d)失败!",fd); 
    }

    return;
}