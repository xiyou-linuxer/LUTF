#ifndef C_COROUTINE_H
#define C_COROUTINE_H
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#if __APPLE__ && __MACH__
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

#define COROUTINE_DEAD 0 // 协程死亡
#define COROUTINE_READY 1 // 协程可运行
#define COROUTINE_RUNNING 2 // 协程运行中
#define COROUTINE_SUSPEND 3 // 协程被切出

#define STACK_SIZE (1024 * 1024)
#define DEFAULT_COROUTINE 16

/*
 * 为什么是72？
 * 因为我们在信号处理中增加了一个局部变量，这样pretcode的偏移就是32字节了。
 * 于是32+40=72！
 */
#define CONTEXT_OFFSET	72
// rip寄存器相对于局部变量a的偏移。注意rip在sigcontext中的偏移是16
#define PC_OFFSET		200
extern unsigned char *buf;
extern struct sigcontext context[2];
extern struct sigcontext *curr_con;
extern unsigned long pc[2];
extern int idx;
extern unsigned char *stack1, *stack2;

// 协程调度器
// 为了ABI兼容，这里故意没有提供具体实现
struct schedule;

typedef void (*coroutine_func)(struct schedule *, void *ud);

// 开启一个协程调度器
struct schedule * coroutine_open(void);

// 关闭一个协程调度器
void coroutine_close(struct schedule *);

// 创建一个协程
int coroutine_new(struct schedule *, coroutine_func, void *ud);

// 切换到对应协程中执行
void coroutine_resume(struct schedule *, int id);

// 返回协程状态
int coroutine_status(struct schedule *, int id);

// 协程是否在正常运行
int coroutine_running(struct schedule *);

// 切出协程
void coroutine_yield(struct schedule *);

void sig_start(int dunno);

void sig_schedule(int signum,siginfo_t *info,void *myact);
struct coroutine;

/**
 * libco 和 coroutine 都使用的是共享栈模型
 * 共享栈: 就是所有协程在运行的时候都使用同一个栈空间,好处是节省空间
 * 非共享栈: 每个协程的栈空间都是独立的,固定大小,好处是协程切换的时候内存不用拷贝来拷贝去,坏处是内存空间浪费
 * 因为栈空间在运行的时候不能随时扩容,否则如果有指针操作执行了占内存,扩容后将会导致指针失效
 * 为了防止指针内存不够用,每个栈都要预先开辟一个足够答的栈空间使用,当然很多协程在实际运行的时候用不了太大
 * 会造成内存的浪费和开辟答内存造成的性能损失
 * 共享栈则是提前开了一个足够大的栈空间(coroutine 默认是1M),所有栈在运行的时候,都使用这个栈空间
 * conroutine 是(本文件235行)形式去设置每个协程的运行时栈
 * C->ctx.uc_stack.ss_sp = S->stack;
 * C->ctx.uc_stack.ss_size = STACK_SIZE;
 */

/**
* 协程调度器的结构体
*/
struct schedule
{
    char stack[STACK_SIZE]; // 运行时栈
    struct sigcontext main2; // xinhaoshangxiawen
    ucontext_t main;	   // 主协程的上下文
    int nco;			   // 当前存活的协程个数
    int cap;			   // 协程管理器的当前最大容量，即可以同时支持多少个协程。可以进行扩容
    int running;		   // 正在运行的协程ID
    struct coroutine **co; // 用于存放协程
};

/*
* 协程结构体
*/
struct coroutine
{
    coroutine_func func;  // 协程所用的函数
//    void () func;
    void *ud;			  // 协程参数
    ucontext_t ctx;		  // 协程上下文

    struct sigcontext con; // xin hao shang xia wen

    struct schedule *sch; // 该协程所属的调度器
    ptrdiff_t cap;		  // stack区已经分配的内存大小
    ptrdiff_t size;		  // 当前协程运行时栈，保存起来后的大小
    int status;			  // 协程当前的状态
    char *stack;		  // 当前协程的保存起来的运行时栈
};
#endif
