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
#include "ngx_c_memory.h"

//来数据时候的处理，当连接上有数据来的时候，本函数会被ngx_epoll_process_events()所调用 
void CSocekt::ngx_wait_request_handler(lpngx_connection_t c) {

    //ngx_log_stderr(errno,"22222222222222222222222.");
    //收包，用的是第二个和第三个参数，必须保证c->precvbuf指向争取的收包位置，保证c->irecvlen指向正确的收包长宽度
    ssize_t reco = recvproc(c, c->precvbuf, c->irecvlen);

    if(reco <= 0) return;

    //走到这里表明成功收到一个字节，就要开始判断收到了多少数据了
    if(c->curStat == _PKG_HD_INIT_) { // 连接建立起来时肯定是这个状态，在ngx_get_connection()中已经把curStat成员赋值成_PKG_HD_INIT了
        
        if (reco == m_iLenPkgHeader) { // 正好收到完整包头，这里拆解包头

            ngx_wait_request_handler_proc_p1(c);   //专门针对包头处理完整的函数去处理

        } else { //因为recvpro 的第三个参数是包头的大小，所以不会存在 reco大于包头的现象

            //收到的包头是不完整的
            c->curStat   = _PKG_HD_RECVING;     //接收包头中，包头不完整，继续接收包头中
            c->precvbuf  = c->precvbuf + reco;  //注意收后续包的内存往后走
            c->irecvlen  = c->irecvlen - reco;  //要收的内容在减少，以确保只收到完整的包头先

        }
    } else if(c->curStat == _PKG_HD_RECVING) { //接收包头中，且包头不完整，继续接收包头该条件才会成立
        if (c->irecvlen == reco) {              //包头完整了

            ngx_wait_request_handler_proc_p1(c);

        } else {           

            // 包头还没有完整，继续收
            c->precvbuf   = c->precvbuf + reco;
            c->irecvlen   = c->irecvlen - reco;

        }

    } else if(c->curStat == _PKG_BD_INIT) { //包头刚好收完，准备接收包体

        if (reco == c->irecvlen) {           //收到的宽度等于要收的宽度，包体也收完整了

            ngx_wait_request_handler_proc_plast(c);
        } else {

            //收到的宽度小于要收的宽度
			c->curStat = _PKG_BD_RECVING;					
			c->precvbuf = c->precvbuf + reco;
			c->irecvlen = c->irecvlen - reco;

        }

    } else if(c->curStat == _PKG_BD_RECVING) {  //接收包体中，包体不完整，继续接收中
        if (c->irecvlen == reco) {               //包体收完整了

            ngx_wait_request_handler_proc_plast(c);

        } else {

            //包体没收完整，继续收
            c->precvbuf = c->precvbuf + reco;
			c->irecvlen = c->irecvlen - reco;
        }

    }

    return;
}


//接收数据专用函数--引入这个函数是为了方便，如果断线，错误之类的，这里直接 释放连接池中连接，然后直接关闭socket，以免在其他函数中还要重复的干这些事
//参数c：连接池中相关连接
//参数buff：接收数据的缓冲区
//参数buflen：要接收的数据大小
//返回值：返回-1，则是有问题发生并且在这里把问题处理完毕了，调用本函数的调用者一般是可以直接return
//        返回>0，则是表示实际收到的字节数
//ssize_t是有符号整型，在32位机器上等同与int，在64位机器上等同与long int，size_t就是无符号型的ssize_t
ssize_t CSocekt::recvproc(lpngx_connection_t c, char *buff, ssize_t buflen) {
    ssize_t n;

    n = recv(c->fd, buff, buflen, 0); //recv是系统函数，最后一个参数flag， 一般为0
    if (n == 0) {
        //客户端关闭【正常四次挥手】，这边直接挥手连接，关闭socket即可
        ngx_close_connection(c);
        return -1;
    }

    //客户端没断，走这里
    if (n < 0) { // 有错误发生

        //EAGAIN 和 EWOULDBLOCK 应该是一样的值，表示没有收到数据，一般来讲，在ET模式下会出现这种错误
        //ET模式下不停地recv  肯定有一个时刻收到这个errno，在LT模式下不应该出现这个错误的
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ngx_log_stderr(errno,"CSocekt::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立，出乎我意料！");
            return -1;
        }

        //EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if (errno == EINTR) {
            //LT模式不该出现这个errno，而且这个其实也不是错误，所以不当做错误处理
            ngx_log_stderr(errno,"CSocekt::recvproc()中errno == EINTR成立，出乎我意料！");
        }

        if (errno == ECONNRESET) {   //#define ECONNRESET 104 /* Connection reset by peer */
            //10054(WSAECONNRESET)--远程程序正在连接的时候关闭会产生这个错误--远程主机强迫关闭了一个现有的连接
            //如果客户端没有正常关闭socket连接，却关闭了整个运行程序【应该是直接给服务器发送rst包而不是4次挥手包完成连接断开】，那么会产生这个错误            
        } else {
             //能走到这里的，都表示错误
            ngx_log_stderr(errno,"CSocekt::recvproc()中发生错误，我打印出来看看是啥错误！");
        }

        ngx_close_connection(c);
        return -1;

    }

    return n;
}

//包头收完整后的处理，我们称为包处理阶段1【p1】：写成函数，方便复用
void CSocekt::ngx_wait_request_handler_proc_p1(lpngx_connection_t c){
    CMemory *p_memory = CMemory::GetInstance();

    LPCOMM_PKG_HEADER pPkgHeader;
    pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;

    unsigned short e_pkgLen;
    e_pkgLen = ntohs(pPkgHeader->pkgLen);  //注意这里网络序转本机序，所有传输到网络上的2字节数据，都要用htons()转成网络序，所有从网络上收到的2字节数据，都要用ntohs()转成本机序
                                            //ntohs/htons的目的就是保证不同操作系统数据之间收发的正确性，【不管客户端/服务器是什么操作系统，发送的数字是多少，收到的就是多少】
    //恶意包或者包错误
    if (e_pkgLen < m_iLenPkgHeader) {

        //报文总长度 < 包头长度，认定非法用户，废包
        //状态和接收位置都复原，这些值都有必要，因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数；
        c->curStat  = _PKG_HD_INIT_;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader; 

    } else if (e_pkgLen > (_PKG_MAX_LENGTH - 1000)) {

        //恶意包，太大，认定非法用户，废包【包头中说这个包总长度这么大，这不行】
        c->curStat = _PKG_HD_INIT_;
        c->precvbuf = c->dataHeadInfo;
        c->irecvlen = m_iLenPkgHeader;

    } else {

        //合法的包头，继续处理
        //现在要分配内存开始收包体，因为包体长度并不是固定的，所以内存肯定要new出来
        char *pTmpBuffer    = (char *)p_memory->AllocMemory(m_iLenPkgHeader + e_pkgLen, false); //分配内存【长度是 消息头长度  + 包头长度 + 包体长度】
        c->ifnewrecvMem     = true;
        c->pnewMemPointer   = pTmpBuffer;  //内存开始指针

        //1）先填写消息头内容
        LPSTRUCT_MSG_HEADER ptmpMsgHeader = (LPSTRUCT_MSG_HEADER)pTmpBuffer;
        ptmpMsgHeader->pConn = c;
        ptmpMsgHeader->iCurrsequence = c->iCurrsequence;  //收到包时的连接池连接序号记录到消息头里来，以备将来用

        //2) 再填写包头内容
        pTmpBuffer += m_iLenMsgHeader;                      //往后跳，跳到消息头，指向包头
        memcpy(pTmpBuffer, pPkgHeader, m_iLenPkgHeader);    //直接把收到的包头拷贝进来
        if (e_pkgLen == m_iLenPkgHeader) { //只有包头没有包体

            //这相当于接收完整了，直接扔入消息队列中
            ngx_wait_request_handler_proc_plast(c);

        } else {    //开始接收包体

            c->curStat  = _PKG_BD_INIT;                     //当前状态发生改变，包头刚好收完，准备接收包体	
            c->precvbuf = pTmpBuffer + m_iLenPkgHeader;     //pTmpBuffer指向包头，这里 + m_iLenPkgHeader后指向包体位置
            c->irecvlen = e_pkgLen - m_iLenPkgHeader;        //e_pkgLen是整个包【包头+包体】大小，-m_iLenPkgHeader【包头】  = 包体
        
        }
    }

    return;
}

//收到一个完整包后的处理【plast表示最后阶段】，放到一个函数中，方便调用
void CSocekt::ngx_wait_request_handler_proc_plast(lpngx_connection_t c){

    //把这段内存存放到消息队列中去
    inMsgRecvQueue(c->pnewMemPointer);

    c->ifnewrecvMem      = false;               //内存不再需要释放，因为你收完整了包，这个包被上边调用inMsgRecvQueue()移入消息队列，那么释放内存就属于业务逻辑去干，不需要回收连接到连接池中干了
    c->pnewMemPointer    = NULL;                
    c->curStat           = _PKG_HD_INIT_;       //收包状态机的状态恢复为原始态，为收下一个包做准备 
    c->precvbuf          = c->dataHeadInfo;     //设置好收包的位置
    c->irecvlen          = m_iLenPkgHeader;     //设置好要接收数据的大小
    return;
}

//当收到一个完整包之后，将完整包入消息队列，这个包在服务器端应该是 消息头+包头+包体 格式 buf这段内存 ： 消息头 + 包头 + 包体
void CSocekt::inMsgRecvQueue(char *buf) {
    
    m_MsgRecvQueue.push_back(buf);

    tmpoutMsgRecvQueue();


     ngx_log_stderr(0,"非常好，收到了一个完整的数据包【包头+包体】！");  

}

//临时函数，用于将Msg中消息干掉
void CSocekt::tmpoutMsgRecvQueue(){

    if (m_MsgRecvQueue.empty()) return;

    int size = m_MsgRecvQueue.size();
    if (size < 1000) return;

    CMemory *p_memory = CMemory::GetInstance();
    int cha = size - 500;
    for (int i = 0; i < cha; i++) {
        char *sTmpMsgBuf = m_MsgRecvQueue.front();
        m_MsgRecvQueue.pop_front();
        p_memory->FreeMemory(sTmpMsgBuf);
    }

    return;

}