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
#include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"

CSocekt::CSocekt() {
    m_ListenPortCount = 1;
    return;
}

//释放函数
CSocekt::~CSocekt() {
    //释放必须的内存
    std::vector<lpngx_listening_t>::iterator pos;	
	for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) //vector
	{		
		delete (*pos); //一定要把指针指向的内存干掉，不然内存泄漏
	}//end for
	m_ListenSocketList.clear(); 

    if (m_pconnections != NULL) { //释放连接池
        delete[] m_pconnections;
    }
    return;
}

//初始化函数【fork()子进程之前干这个事】
//成功返回true，失败返回false
bool CSocekt::Initialize() {
    ReadConf();
    bool reco = ngx_open_listening_sockets();
    return reco;
}

void CSocekt::ReadConf(){
    CConfig *p_config = CConfig::GetInstance();
    m_worker_connections = p_config->GetIntDefault("worker_connections", m_worker_connections); //epoll连接的最大项数
    m_ListenPortCount    = p_config->GetIntDefault("ListenPortCount", m_ListenPortCount);       //取得要监听的端口数量
}

//监听端口【支持多个端口】，这里遵从nginx的函数命名
//在创建worker进程之前就要执行这个函数；
bool CSocekt::ngx_open_listening_sockets() {
    CConfig *p_config = CConfig::GetInstance();
    m_ListenPortCount = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount); //取得要监听的端口数量
    
    int                isock;                //socket
    struct sockaddr_in serv_addr;            //服务器的地址结构体
    int                iport;                //端口
    char               strinfo[100];         //临时字符串 
   
    //初始化相关
    memset(&serv_addr,0,sizeof(serv_addr));         //先初始化一下
    serv_addr.sin_family = AF_INET;                 //选择协议族为IPV4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //监听本地所有的IP地址；INADDR_ANY表示的是一个服务器上所有的网卡（服务器可能不止一个网卡）多个本地ip地址都进行绑定端口号，进行侦听。

    for(int i = 0; i < m_ListenPortCount; i++) {    //要监听这么多个端口       
        //参数1：AF_INET：使用ipv4协议，一般就这么写
        //参数2：SOCK_STREAM：使用TCP，表示可靠连接【相对还有一个UDP套接字，表示不可靠连接】
        //参数3：给0，固定用法，就这么记
        isock = socket(AF_INET,SOCK_STREAM,0); //系统函数，成功返回非负描述符，出错返回-1
        if(isock == -1){
            ngx_log_stderr(errno,"CSocekt::Initialize()中socket()失败,i=%d.",i);
            //其实这里直接退出，那如果以往有成功创建的socket呢？就没得到释放吧，当然走到这里表示程序不正常，应该整个退出，也没必要释放了 
            return false;
        }

        //setsockopt（）:设置一些套接字参数选项；
        //参数2：是表示级别，和参数3配套使用，也就是说，参数3如果确定了，参数2就确定了;
        //参数3：允许重用本地地址
        //设置 SO_REUSEADDR,在这里出现主要是解决TIME_WAIT这个状态导致bind()失败的问题
        int reuseaddr = 1;  //1:打开对应的设置项
        if(setsockopt(isock,SOL_SOCKET, SO_REUSEADDR,(const void *) &reuseaddr, sizeof(reuseaddr)) == -1) {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败,i=%d.",i);
            close(isock); //无需理会是否正常执行了                                                  
            return false;
        }
        //设置该socket为非阻塞
        if(setnonblocking(isock) == false) {                
            ngx_log_stderr(errno,"CSocekt::Initialize()中setnonblocking()失败,i=%d.",i);
            close(isock);
            return false;
        }

        //设置本服务器要监听的地址和端口，这样客户端才能连接到该地址和端口并发送数据        
        strinfo[0] = 0;
        sprintf(strinfo,"ListenPort%d",i);
        iport = p_config->GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);   //in_port_t其实就是uint16_t

        //绑定服务器地址结构体
        if(bind(isock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
            ngx_log_stderr(errno,"CSocekt::Initialize()中bind()失败,i=%d.",i);
            close(isock);
            return false;
        }
        
        //开始监听
        if(listen(isock,NGX_LISTEN_BACKLOG) == -1) {
            ngx_log_stderr(errno,"CSocekt::Initialize()中listen()失败,i=%d.",i);
            close(isock);
            return false;
        }

        //可以，放到列表里来
        lpngx_listening_t p_listensocketitem = new ngx_listening_t;     //注意前边类型是指针，后边类型是一个结构体
        memset(p_listensocketitem,0,sizeof(ngx_listening_t));           //注意后边用的是 ngx_listening_t而不是lpngx_listening_t
        p_listensocketitem->port = iport;                               //记录下所监听的端口号
        p_listensocketitem->fd   = isock;                               //套接字木柄保存下来   
        ngx_log_error_core(NGX_LOG_INFO,0,"监听%d端口成功!",iport);       //显示一些信息到日志中
        m_ListenSocketList.push_back(p_listensocketitem);               //加入到队列中
    } //end for(int i = 0; i < m_ListenPortCount; i++)    
    return true;
}

//设置socket连接为非阻塞模式【这种函数的写法很固定】：非阻塞，【不断调用，不断调用这种：拷贝数据的时候是阻塞的】
bool CSocekt::setnonblocking(int sockfd) 
{    
    int nb=1; //0：清除，1：设置  
    if(ioctl(sockfd, FIONBIO, &nb) == -1) {  //FIONBIO：设置/清除非阻塞I/O标记：0：清除，1：设置
        return false;
    }
    return true;

    //如下也是一种写法，跟上边这种写法其实是一样的，但上边的写法更简单
    /* 
    //fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    //参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    if(opts < 0) 
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_GETFL)失败.");
        return false;
    }
    opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    if(fcntl(sockfd, F_SETFL, opts) < 0) 
    {
        ngx_log_stderr(errno,"CSocekt::setnonblocking()中fcntl(F_SETFL)失败.");
        return false;
    }
    return true;
    */
}

//关闭socket
void CSocekt::ngx_close_listening_sockets() {
    for(int i = 0; i < m_ListenPortCount; i++) {  //要关闭这么多个监听端口  
        //ngx_log_stderr(0,"端口是%d,socketid是%d.",m_ListenSocketList[i]->port,m_ListenSocketList[i]->fd);
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO,0,"关闭监听端口%d!",m_ListenSocketList[i]->port); //显示一些信息到日志中
    }//end for(int i = 0; i < m_ListenPortCount; i++)
    return;
}

//epoll初始化，子进程中进行
int CSocekt::ngx_epoll_init() {
    m_epollhandle = epoll_create(m_worker_connections);
    if (m_epollhandle == -1) {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2);
    }

    //创建连接池，创建出来之后，这个东西后续用于处理所有客户端的连接
    m_connection_n = m_worker_connections;    //记录当前连接池连接总数

    //连接池【数组，每个元素都是一个对象】
    m_pconnections = new ngx_connection_t[m_connection_n];

    int i = m_connection_n;
    lpngx_connection_t next = NULL;
    lpngx_connection_t c = m_pconnections;   //连接池数组首地址
    do {
         i--;                                //从屁股后面开始遍历的

         c[i].data = next;                   //设置连接对象的next指针，注意第一次循环时，next为NULL
         c[i].fd = -1;                       //初始化连接，无socket和该连接池中的连接【对象】绑定
         c[i].instance = 1;                  //失效标志位设置为1【失效】
         c[i].iCurrsequence = 0;             //当前序号统一从0开始

         next = &c[i];

    } while(i); //循环到i为0，即数组首地址

    m_pfree_connections = next;             //设置空闲连接链表头指针,因为现在next指向c[0]，注意现在整个链表都是空的
    m_free_connection_n = m_connection_n;   //空闲连接链表长度，因为现在整个链表都是空的，这两个长度相等；

    //遍历所有的监听socket，为每一个socket增加一个连接池中的连接【就是让一个socket和一个内存绑定，以方便记录socket相关的数据、状态等】
    std::vector<lpngx_listening_t>::iterator pos;
    for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos) {
        c = ngx_get_connection((*pos)->fd);   //从连接池中获取一个空闲连接对象
        if (c == NULL) {
            ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2);
        }
        c->listening = (*pos);   //连接对象，和监听对象关联，方便通过连接对象找到监听对象
        (*pos)->connection = c;  //监听对象 和连接对象关联，方便通过监听对象找连接对象

        //对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三次握手，所以监听端口关心的就是读事件
        c->rhandler = &CSocekt::ngx_event_accept;

        //往监听socket上增加监听事件，从而开始让监听端口履行职责【如果不加这行，虽然端口能连上，但不会触发ngx_epoll_process_events()里边的epoll_wait()往下走】
        if (ngx_epoll_add_event( (*pos)->fd,     //socekt句柄
                                1,0,             //读，写【只关心读事件，所以参数2：readevent=1,而参数3：writeevent=0】
                                0,               //其他补充标记
                                EPOLL_CTL_ADD,   //事件类型【增加，还有删除/修改】
                                c                 //连接池中的连接 
                                ) == -1) {
            exit(2);
        }
    }
    return 1;
}


/*
    epoll增加事件，可能被ngx_epoll_init()等函数调用
    fd:句柄，一个socket
    readevent：表示是否是个读事件，0是，1不是
    writeevent：表示是否是个写事件，0是，1不是
    otherflag：其他需要额外补充的标记，弄到这里
    eventtype：事件类型  ，一般就是用系统的枚举值，增加，删除，修改等;
    c：对应的连接池中的连接的指针
    返回值：成功返回1，失败返回-1；
*/
int CSocekt::ngx_epoll_add_event(int fd,
                                 int readevent, int writeevent,
                                 uint32_t otherflag,
                                 uint32_t eventtype,
                                 lpngx_connection_t c) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    if (readevent == 1) {
        /*
            EPOLLIN读事件，也就是read ready【客户端三次握手连接进来，也属于一种可读事件】
            EPOLLRDHUP 客户端关闭连接，断连
            不用加EPOLLERR，只用EPOLLRDHUP即可，EPOLLERR/EPOLLRDHUP 实际上是通过触发读写事件进行读写操作recv write来检测连接异常

        */
        ev.events = EPOLLIN|EPOLLRDHUP;
    } else {
        //其他事件类型待处理
    
    }

    if (otherflag != 0) {
        ev.events |= otherflag;
    }

    //指针的最后一位【二进制位】肯定不是1，所以 和 c->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的c->instance值取到
    ev.data.ptr = (void *)((uintptr_t)c | c->instance);

    if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readevent,writeevent,otherflag,eventtype);
        return -1;
    }
    return 1;

}


/*
    开始获取发生的事件消息
    参数unsigned int timer：epoll_wait()阻塞的时长，单位是毫秒；
    返回值，1：正常返回  ,0：有问题返回，一般不管是正常还是问题返回，都应该保持进程继续运行
    本函数被ngx_process_events_and_timers()调用，而ngx_process_events_and_timers()是在子进程的死循环中被反复调用
*/
int CSocekt::ngx_epoll_process_event(int timr) {

    /*
        等待事件，事件会返回到m_events里，最多返回NGX_MAX_EVENTS个事件【因为我只提供了这些内存】；
        如果两次调用epoll_wait()的事件间隔比较长，则可能在epoll的双向链表中，积累了多个事件，所以调用epoll_wait，可能取到多个事件
        阻塞timer这么长时间除非：
            a)阻塞时间到达 
            b)阻塞期间收到事件【比如新用户连入】会立刻返回
            c)调用时有事件也会立刻返回d)如果来个信号，比如你用kill -1 pid测试
        如果timer为-1则一直阻塞，如果timer为0则立即返回，即便没有任何事件
        返回值：有错误发生返回-1，错误在errno中，比如你发个信号过来，就返回-1，错误信息是(4: Interrupted system call)
               如果你等待的是一段时间，并且超时了，则返回0；
               如果返回>0则表示成功捕获到这么多个事件【返回值里】
    */

    int events = epoll_wait(m_epollhandle, m_events, NGX_MAX_EVENTS, timr);

    if (events == -1) {

        //有错误发生，发送某个信号给本进程就可以导致这个条件成立，而且错误码根据观察是4；
        //#define EINTR  4，EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
               //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if (errno == EINTR) {
            //收到信号，直接返回
            ngx_log_error_core(NGX_LOG_INFO,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 1;
        } else {
            //这被认为应该是有问题，记录日志
            ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 0;
        }
    }

    if (events == 0) {          //超时但没事件来
        if (timr != -1) {       //阻塞时间到了，正常返回
            return 1;
        }
        //无限等待【所以不存在超时】，但却没返回任何事件，这应该不正常有问题        
        ngx_log_error_core(NGX_LOG_ALERT,0,"CSocekt::ngx_epoll_process_events()中epoll_wait()没超时却没返回任何事件!"); 
        return 0;

    }

    //会惊群，一个telnet上来，4个worker进程都会被惊动，都执行下边这个


    //下面属于正常的事件到来
    lpngx_connection_t  c;
    uintptr_t           instance;
    uint32_t            revents;
    for (int i = 0; i < events; i++) {
        c = (lpngx_connection_t)(m_events[i].data.ptr);             //ngx_epoll_add_event()给进去的，这里能取出来
        instance = (uintptr_t) c & 1;                               //将地址的最后一位取出来，用instance变量标识, 见ngx_epoll_add_event，该值是当时随着连接池中的连接一起给进来的

        c = (lpngx_connection_t) ((uintptr_t)c & (uintptr_t) ~1);   //最后1位干掉，得到真正的c地址

        if (c->fd == -1) {

            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭，那我们应该会把c->fd设置为-1；
            //第二个事件照常处理
            //第三个事件，假如这第三个事件，也跟第一个事件对应的是同一个连接，那这个条件就会成立；那么这种事件，属于过期事件，不该处理
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了fd=-1的过期事件:%p.",c); 
            continue; 
        }

        if (c->instance != instance) {
            //--------------------以下这些说法来自于资料--------------------------------------
            //什么时候这个条件成立呢？【换种问法：instance标志为什么可以判断事件是否过期呢？】
            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭【麻烦就麻烦在这个连接被服务器关闭上了】，但是恰好第三个事件也跟这个连接有关；
            //因为第一个事件就把socket连接关闭了，显然第三个事件我们是不应该处理的【因为这是个过期事件】，若处理肯定会导致错误；
            //那我们上述把c->fd设置为-1，可以解决这个问题吗？ 能解决一部分问题，但另外一部分不能解决，不能解决的问题描述如下【这么离奇的情况应该极少遇到】：

            //a)处理第一个事件时，因为业务需要，我们把这个连接【假设套接字为50】关闭，同时设置c->fd = -1;并且调用ngx_free_connection将该连接归还给连接池；
            //b)处理第二个事件，恰好第二个事件是建立新连接事件，调用ngx_get_connection从连接池中取出的连接非常可能就是刚刚释放的第一个事件对应的连接池中的连接；
            //c)又因为a中套接字50被释放了，所以会被操作系统拿来复用，复用给了b)【一般这么快就被复用也是醉了】；
            //d)当处理第三个事件时，第三个事件其实是已经过期的，应该不处理，那怎么判断这第三个事件是过期的呢？ 【假设现在处理的是第三个事件，此时这个 连接池中的该连接 实际上已经被用作第二个事件对应的socket上了】；
                //依靠instance标志位能够解决这个问题，当调用ngx_get_connection从连接池中获取一个新连接时，我们把instance标志位置反，所以这个条件如果不成立，说明这个连接已经被挪作他用了；
            ngx_log_error_core(NGX_LOG_DEBUG,0,"CSocekt::ngx_epoll_process_events()中遇到了instance值改变的过期事件:%p.",c); 
            continue;

        }

        revents = m_events[1].events;           //取出事件类型
        if (revents & (EPOLLERR | EPOLLHUP)) {  //例如对方close掉套接字，这里会感应到【换句话说：如果发生了错误或者客户端断连】
            revents |= EPOLLIN|EPOLLOUT;        //EPOLLIN：表示对应的链接上有数据可以读出（TCP链接的远端主动关闭连接，也相当于可读事件，因为本服务器小处理发送来的FIN包）
                                                //EPOLLOUT：表示对应的连接上可以写入数据发送【写准备好】
        }

        if (revents & EPOLLIN) {                 //如果是读事件

            (this->*(c->rhandler))(c);          //如果新连接进入，这里执行的应该是CSocekt::ngx_event_accept(c)】
                                                //如果是已经连入，发送数据到这里，则这里执行的应该是 CSocekt::ngx_wait_request_handler

        }

        if (revents & EPOLLOUT) {               //如果是写事件

        }
    }


    return 1;

}