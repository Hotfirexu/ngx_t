#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include <vector>

#define NGX_LISTEN_BACKLOG 511  

typedef struct ngx_listening_s {
    int  port;     //监听的端口号
    int  fd;      //套接字句柄socket

}ngx_listening_t,*lpngx_listening_t;

class CSocekt {
public:
    CSocekt();
    virtual ~CSocekt();

public:
    virtual bool Initialize();
private:
    bool ngx_open_listening_sockets();
    void nhx_close_listening_sockets();
    bool setnonblocking(int sockfd);

private:
    int                             m_ListenPortCount;
    std::vector<lpngx_listening_t>  m_ListenSocketList;

};


#endif