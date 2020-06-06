#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include <vector>
#include <list>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "ngx_comm.h"

#define NGX_LISTEN_BACKLOG 511    //已完成连接队列
#define NGX_MAX_EVENTS 512        //epoll_wait一次最多接收这么多个事件

typedef struct ngx_listening_s  ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s ngx_connection_t,*lpngx_connection_t;
typedef struct CSocekt          CSocekt;


typedef void (CSocekt::*ngx_event_handler_pt)(lpngx_connection_t c);//定义成员函数指针

struct ngx_listening_s {
    int                 port;       //监听的端口号
    int                 fd;         //套接字句柄socket
    lpngx_connection_t  connection; //连接池中的一个连接，这是一个指针

};

//该结构表示一个TCP连接【客户端发起的、Nginx服务器被动接收的TCP连接】
struct ngx_connection_s {
    int                     fd;                 //套接字句柄socket
    lpngx_listening_t       listening;          //如果这个连接分配给一个监听套接字，那么这个里边指向监听套接字对应的那个lpngx_listening_t的内存首地址

    unsigned                instance:1;         //【位域】失效标志位：0：有效，1：失效
    uint64_t                iCurrsequence;      // 引入的一个序号，每次分配出去时+1，此法也有可能在一定程度上检测错包废包
    struct sockaddr         s_sockaddr;         //保存对方地址信息的

    uint8_t                 w_ready;            //写准备好标记
 
    ngx_event_handler_pt    rhandler;           //读事件的相关处理方法
    ngx_event_handler_pt    whandler;           //写事件的相关处理办法

    //和收包相关
    unsigned char           curStat;            //当前收包的状态
    char                    dataHeadInfo[_DATA_BUFSIZE_]; //用于保存收到的数据的包头信息
    char                    *precvbuf;          // 接收数据的缓冲区的头指针，对接收不完全的包非常有用，
    unsigned int            irecvlen;           // 要收到多少数据，由这个变量指定，和precvbuf配套使用

    bool                    ifnewrecvMem;       // 如果成功的收到了包头，那么就需要分配内存开始保存，包头 + 包体 + 内容
                                                // 这个标记用来标记是否new过内存，用于释放
    char                    *pnewMemPointer;    // new出来的用于收包的内存首地址， 和ifnewrecvMem配套使用

    lpngx_connection_t      data;               //指针，指向下一个本类型对象，用于把空闲的连接池对象串联起来构成一个单向链表，方便取用
};

// 消息头，目的是当接收到数据包时，额外记录一些内容以备将来使用
typedef struct _STRUCT_MSG_HEADER {
    lpngx_connection_t  pConn;              // 记录对应的链接，注意这个是个指针
    uint64_t            iCurrsequence;      // 收到数据包时记录对应连接的序号，将来能用于比较是否连接已经作废


}STRUCT_MSG_HEADER, *LPSTRUCT_MSG_HEADER;

class CSocekt {
public:
    CSocekt();
    virtual ~CSocekt();

public:
    virtual bool Initialize();

public:
    int ngx_epoll_init();    //epoll初始化
    /*
        epoll增加事件
    */
    int ngx_epoll_add_event(int fd, int readevent, int writeevent, uint32_t otherflag, uint32_t eventype, lpngx_connection_t c);
    int ngx_epoll_process_event(int timer);   //epoll等待接收和处理事件
private:

    void ReadConf();
    bool ngx_open_listening_sockets();
    void ngx_close_listening_sockets();
    bool setnonblocking(int sockfd);

    //业务处理函数的handler
    void ngx_event_accept(lpngx_connection_t oldc);         //建立新连接
    void ngx_wait_request_handler(lpngx_connection_t c);    //设置数据来时的读处理函数
    void ngx_close_connection(lpngx_connection_t c); 

    ssize_t recvproc(lpngx_connection_t c, char *buff, ssize_t buflen);  //接收从客户端来的数据专用函数
    void ngx_wait_request_handler_proc_p1(lpngx_connection_t c);          // 包头收完整后的处理，包处理阶段1
    void ngx_wait_request_handler_proc_plast(lpngx_connection_t c);       // 收到一个完整包后的处理
    void inMsgRecvQueue(char *buf);                                         // 收到一个完整消息后，放入消息队列
    void tmpoutMsgRecvQueue();
    void clearMsgRecvQueue();

    size_t ngx_sock_ntop(struct sockaddr *sa, int port, u_char *text, size_t len);//获取对端信息

    //连接池 或 连接相关
    lpngx_connection_t ngx_get_connection(int isock); //从连接池中获取一个空闲连接
    void ngx_free_connection(lpngx_connection_t c);   //归还参数c所代表的连接到连接池中

private:
    int                             m_worker_connections;       //epoll连接的最大项数
    int                             m_ListenPortCount;          //监听端口数
    int                             m_epollhandle;              //epoll_create返回的句柄

    lpngx_connection_t              m_pconnections;             //连接池的首地址
    lpngx_connection_t              m_pfree_connections;        //空闲连接链表头，连接池中总是有某些连接被占用，为了快速在池中找到一个空间的连接

    int                             m_connection_n;             //当前进程中所有连接对象的总数
    int                             m_free_connection_n;        //连接池中可用的连接数
    std::vector<lpngx_listening_t>  m_ListenSocketList;         //监听套接字队列
    struct epoll_event              m_events[NGX_MAX_EVENTS];   //用于在epoll_wait()中承载返回的所发生的事件

    //一些和网络通讯有关的成员变量
    size_t                          m_iLenPkgHeader;            //sizeof(COMM_PKG_HEADER);	
    size_t                          m_iLenMsgHeader;            //sizeof(STRUC_MSG_HEADER);

    //消息队列
	std::list<char *>                m_MsgRecvQueue;             //接收数据消息队列 

};


#endif