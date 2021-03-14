#include "coroutine.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

struct args {
	int n;
};

void thread1(struct schedule* S, void *ud) {

    while (1){
        printf("thread1\n");
        sleep(1);
    }
}

void thread2(struct schedule* S, void *ud) {

    while (1){
        printf("thread2\n");
        sleep(1);
    }
}

int main() {
	// 创建一个协程调度器s,此调度器用来统一管理全部的协程
	struct schedule * S = coroutine_open();

/**************************************************/

    struct args arg1 = { 0 };
    struct args arg2 = { 100 };

//    // 创建两个协程
    int co1 = coroutine_new(S, thread1, &arg1);
    int co2 = coroutine_new(S, thread2, &arg2);

//    printf("process %d %d\n", co1, co2);

    struct sigaction act;
    union sigval mysig;
    mysig.sival_ptr = S;

    int sig = SIGALRM;
    pid_t pid = getpid();
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = sig_schedule; // 三参数信号处理函数
    act.sa_flags = SA_SIGINFO;  // 信息传递开关

    if(sigaction(sig,&act,NULL) < 0) {
        printf("install sigal error\n");
    }
//    pc[0] = (unsigned long)thread1;
//    pc[1] = (unsigned long)thread2;

    while(1) {
        printf("wait for the signal\n");
        sigqueue(pid,sig,mysig);//向本进程发送信号，并传递附加信息
        sleep(2);
    }
    // 关闭协程调度器
	coroutine_close(S);

	return 0;
}

