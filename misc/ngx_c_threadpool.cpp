

#include <stdarg.h>
#include <unistd.h>  //usleep

#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_threadpool.h"
#include "ngx_c_memory.h"
#include "ngx_macro.h"


//静态成员初始化
pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;
bool CThreadPool::m_shutdown = false;


CThreadPool::CThreadPool() {
    m_iRunningThreadNum = 0;
    m_iLastEmgTime = 0;
}

CThreadPool::~CThreadPool() {}

bool CThreadPool::Create(int threadNum) {
    ThreadItem *pNew;
    int err;

    m_iThreadNum = threadNum;     //保存要创建的线程数

    for (int i = 0; i < m_iThreadNum; ++ i) {
        m_threadVector.push_back(pNew = new ThreadItem(this));          //创建一个新线程对象，并加入到容器中
        err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);    //创建线程
        if(err != 0) {
            //创建线程有错
            ngx_log_stderr(err,"CThreadPool::Create()创建线程%d失败，返回的错误码为%d!",i,err);
            return false;
        }
    }

    //必须保证每个线程都启动并运行到pthread_cond_wait()，本函数才返回，只有这样，这几个线程才能进行后续的正常工作 
    std::vector<ThreadItem *>::iterator iter;
lblfor:
    for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter ++) {
        if ((*iter)->ifrunning == false) { //这个条件保证所有线程完全启动起来，以保证整个线程池中的线程正常工作
            usleep(100 * 10000);
            goto lblfor;
        }
    }

    return true;

}

//线程入口函数，当用pthread_create()创建线程后，这个ThreadFunc()函数都会被立即执行
//这是个静态函数，是不存在this指针的
void* CThreadPool::ThreadFunc(void* threadData) {
    
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool *pThreadPoolObj  = pThread->_pThis;

    char *jobbuf = NULL;
    CMemory *p_memory = CMemory::GetInstance();
    int err;

    pthread_t tid = pthread_self();         //获取线程本身id，以方便调试打印信息等
    while (true) {

        //线程用pthread_mutex_lock函数去锁定指定的mutex变量，若该mutex已经被另一个线程锁定了，该调用将会阻塞线程直到mutex被解锁
        err = pthread_mutex_lock(&m_pthreadMutex);
        if (err != 0) ngx_log_stderr(err,"CThreadPool::ThreadFunc()pthread_mutex_lock()失败，返回的错误码为%d!",err);//有问题，要及时报告

        //pthread_cond_wait()是个值得注意的函数，调动一次pthread_cond_signal()可能会唤醒多个线程【惊群】【多处理器上会存在这种情况】
        //pthread_cond_wait()这里，如果只有一条消息却唤醒了多个线程，那么其中一个线程拿到消息，用了while之后，其余没拿到消息的线程会继续卡在这里
        while ((jobbuf == g_socekt.outMsgRecvQueue()) == NULL && m_shutdown == false) {
            if (pThread->ifrunning == false)
                pThread->ifrunning = true;

            //刚开始执行pthread_cond_wait的时候，会卡在这里，但是m_pthreadMutex却是会被释放掉
            //整个服务器程序初始化的时候，所有线程必然是卡在这里等待的
            pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex);
        }

        //走到这里的线程肯定是拿到消息队列里的消息了，  或者 m_shutdown == true

        err = pthread_mutex_unlock(&m_pthreadMutex);
        if(err != 0)  ngx_log_stderr(err,"CThreadPool::ThreadFunc()pthread_cond_wait()失败，返回的错误码为%d!",err);//有问题，要及时报告

        if (m_shutdown) {
            if (jobbuf == NULL) {
                p_memory->FreeMemory(jobbuf);
            }
            break;
        }

        ++pThreadPoolObj->m_iRunningThreadNum;

        p_memory->FreeMemory(jobbuf);
        --pThreadPoolObj->m_iRunningThreadNum;
    } //end while(true)

    return (void*)0;


}

//停止所有的线程
void CThreadPool::StopAll() {

    //已经调用过了，就不需要重复调用了
    if (m_shutdown == true) return;

    m_shutdown = true;

    //唤醒等待该条件的所有的线程，一定要在改变条件状态之后再给所有线程发信号
    int err = pthread_cond_broadcast(&m_pthreadCond);
    if (err != 0) {
        //这肯定是有问题，要打印紧急日志
        ngx_log_stderr(err,"CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d!",err);
        return;
    }

    std::vector<ThreadItem*>::iterator iter;
    for (iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter) {
        pthread_join((*iter)->_Handle,NULL); //等待线程终止
    }

    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);

    for (iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter) {
        if (*iter) delete *iter;
    }

    m_threadVector.clear();
    ngx_log_stderr(0,"CThreadPool::StopAll()成功返回，线程池中线程全部正常结束!");
    return;
}


//来任务了，调一个线程池中的线程来干活
void CThreadPool::Call(int irmqc) {

    //唤醒一个等待该条件的线程，也就是可以唤醒卡在pthread_cond_wait()的线程
    int err = pthread_cond_signal(&m_pthreadCond);
    if(err != 0 ) {
        //这是有问题啊，要打印日志啊
        ngx_log_stderr(err,"CThreadPool::Call()中pthread_cond_signal()失败，返回的错误码为%d!",err);
    }

    //(1)如果当前的工作线程全部都忙，则要报警
    if (m_iThreadNum == m_iRunningThreadNum) {  //线程池中线程总量，跟当前正在干活的线程数量一样，说明所有线程都忙碌起来，线程不够用了
        time_t currtime = time(NULL);
        if (currtime - m_iLastEmgTime > 10) {   //最少间隔10秒钟才报一次线程池中线程不够用的问题
            m_iLastEmgTime = currtime;
            ngx_log_stderr(0,"CThreadPool::Call()中发现线程池中当前空闲线程数量为0，要考虑扩容线程池了!");
        }
    }

    return;
}