/*************************************************************************
> Author       : doc.xu
> Mail         : jianx_xu@163.com
> File Name    : nginx.c
> Created Time : 2020-04-11 16:28:09
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

void sig_quit(int signo) { //信号处理函数
    printf("收到了SIGQUIT信号了！\n");
}

int main(int argc, char *const *argv) {
    

    sigset_t newmask, oldmask; //信号集，新的信号集，原有的信号集
    if (signal(SIGQUIT, sig_quit) == SIG_ERR) {
        printf("无法捕捉SIFQUIT信号！\n");
        exit(1);
    }

    sigemptyset(&newmask); //newmask信号集中所有的信号清零（表示这些信号都没来）
    sigaddset(&newmask, SIGQUIT); //设置newmask信号集中的SIGQUIT信号位为1，说白了，再来SIGQUIT信号时，进程就收不到了

    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0) {//第一个参数用了SIG_BLOCK表明设置 进程 新的信号屏蔽字 为 “当前信号屏蔽字 和 第二个参数指向的信号集的并集
                                                     //一个 ”进程“ 的当前信号屏蔽字，刚开始全部都是0的；所以相当于把当前 "进程"的信号屏蔽字设置成 newmask（屏蔽了SIGQUIT)；
                                                      //第三个参数不为空，则进程老的(调用本sigprocmask()之前的)信号集会保存到第三个参数里，用于后续，这样后续可以恢复老的信号集给线程
        printf("sigprocmask(SIG_BLOCK失败！\n)");
        exit(1);
    }
    printf("休息10秒。\n");
    sleep(10);

    if (sigismember(&newmask, SIGQUIT)) { //测试一个指定的信号位是否被置位(为1)，测试的是newmask
        printf("SIGQUIT信号被屏蔽了！\n");
    } else {
        printf("SIGQUIT信号没有被屏蔽！\n");
    }

    if (sigismember(&newmask, SIGHUP)) { //测试另外一个指定的信号位是否被置位,测试的是newmask
        printf("SIGHUP信号被屏蔽了！\n");
    } else {
        printf("SIGHUP信号没有被屏蔽！\n");
    }


    //现在我要取消对SIGQUIT信号的屏蔽(阻塞)--把信号集还原回去
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0) {  //第一个参数用了SIGSETMASK表明设置 进程  新的信号屏蔽字为 第二个参数 指向的信号集，第三个参数没用
        printf("sigprocmask(SIG_SETMASK)失败!\n");
        exit(1);
    }

    printf ("sigprocmask(SIG_SETMASK)成功!\n");


    if(sigismember(&oldmask,SIGQUIT))  //测试一个指定的信号位是否被置位,这里测试的当然是oldmask
    {
        printf("SIGQUIT信号被屏蔽了!\n");
    }
    else
    {
        printf("SIGQUIT信号没有被屏蔽，您可以发送SIGQUIT信号了，我要sleep(10)秒钟!!!!!!\n");
        int mysl = sleep(10);
        if(mysl > 0)
        {
            printf("sleep还没睡够，剩余%d秒\n",mysl);
        }
    }
    printf("再见了!\n");
    return 0;
}
