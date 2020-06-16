#ifndef __NGX_THREADPOOL_H__
#define __NGX_THREADPOOL_H__


#include <vector>
#include <pthread.h>
#include <atomic>

//线程池相关类
class CThreadPool {

public:
    CThreadPool();
    ~CThreadPool();

public:
    bool Create(int threadNum);
    void StopAll();
    void Call(int irmqc);                       //来任务，从线程池中调一个线程来干活

private:
    static void* ThreadFunc(void *threadData);  //新线程的回调函数

private:

    // 定义一个 线程池中的 线程 的结构，以后可能做一些统计之类的 功能扩展
    // 引入这么个结构来 代表线程 感觉更方便一些； 
    struct ThreadItem {
        pthread_t   _Handle;            //线程句柄
        CThreadPool  *_pThis;            //记录线程池的指针
        bool        ifrunning;          //标记是否正式启动起来，启动起来后，才允许调用StopAll来释放

        ThreadItem(CThreadPool *pthis):_pThis(pthis),ifrunning(false){}
        ~ThreadItem(){}
    };

private:
    static  pthread_mutex_t    m_pthreadMutex;      //线程同步互斥量/也叫线程同步锁
    static  pthread_cond_t     m_pthreadCond;       //线程同步条件变量
    static  bool               m_shutdown;          //线程退出标志，false不退出，true退出

    int                        m_iThreadNum;        //要创建的线程数量

    std::atomic<int>           m_iRunningThreadNum; //线程数，运行中的线程数，原子操作
    time_t                     m_iLastEmgTime;      //上次发生下城不够用的时间，防止报警台频繁
    std::vector<ThreadItem *>  m_threadVector;      //线程容器，容器里就是各个线程了

};

#endif
