#ifndef __TIMER_H
#define __TIMER_H

#include "stdint.h"
#include "sync.h"

extern uint64_t ticks;
extern int clock_granularity;  // 每隔多少毫秒触发一次调度
#define CONFIG_BASE_SMALL 0    // TVN_SIZE=64  TVR_SIZE=256
#define TVN_BITS (CONFIG_BASE_SMALL ? 4 : 6)
#define TVR_BITS (CONFIG_BASE_SMALL ? 6 : 8)
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)
#define MAX_TVAL ((unsigned long)((1ULL << (TVR_BITS + 4*TVN_BITS)) - 1))

#define TIME_AFTER(a,b) ((long)(b) - (long)(a) < 0)
#define TIME_BEFORE(a,b) TIME_AFTER(b,a)
#define TIME_AFTER_EQ(a,b) ((long)(a) - (long)(b) >= 0)
#define TIME_BEFORE_EQ(a,b) TIME_AFTER_EQ(b,a)

typedef struct LIST_TIMER
{
    struct LIST_TIMER *pPrev;
    struct LIST_TIMER *pNext;
} LISTTIMER, *LPLISTTIMER;

typedef struct TIMER_NODE
{
    struct LIST_TIMER ltTimer;  // 定时器双向链表的入口
    uint32_t uExpires;          // 定时器到期时间
    uint32_t uPeriod;           // 定时器触发后，再次触发的间隔时长。如果为 0，表示该定时器为一次性的
    void (*timerFn)(void *);    // 定时器回调函数
    void *pParam;               // 回调函数的参数
} TIMERNODE, *LPTIMERNODE;

typedef struct TIMER_MANAGER
{
    struct lock lock;
    uint32_t uExitFlag;           // 线程退出标识(0:Continue, other: Exit)
    uint32_t uJiffies;            // 基准时间(当前时间)，单位：毫秒
    struct LIST_TIMER arrListTimer1[TVR_SIZE];  // 1 级时间轮。在这里表示存储未来的 0 ~ 255 毫秒的计时器。tick 的粒度为 1 毫秒
    struct LIST_TIMER arrListTimer2[TVN_SIZE];  // 2 级时间轮。存储未来的 256 ~ 256*64-1 毫秒的计时器。tick 的粒度为 256 毫秒
    struct LIST_TIMER arrListTimer3[TVN_SIZE];  // 3 级时间轮。存储未来的 256*64 ~ 256*64*64-1 毫秒的计时器。tick 的粒度为 256*64 毫秒
    struct LIST_TIMER arrListTimer4[TVN_SIZE];  // 4 级时间轮。存储未来的 256*64*64 ~ 256*64*64*64-1 毫秒的计时器。tick 的粒度为 256*64*64 毫秒
    struct LIST_TIMER arrListTimer5[TVN_SIZE];  // 5 级时间轮。存储未来的 256*64*64*64 ~ 256*64*64*64*64-1 毫秒的计时器。tick 的粒度为 256*64*64 毫秒
} TIMERMANAGER, *LPTIMERMANAGER;
extern LPTIMERMANAGER timer_manager;
/**
 * interrupt_timer_handler - 时钟中断处理函数
 * **/
void interrupt_timer_handler(unsigned long* a);

// 创建定时器管理器
void timer_init(void);

// 删除定时器管理器
void destroy_timer_manager();

// 创建一个定时器。fnTimer 回调函数地址。pParam 回调函数的参数。uDueTime 首次触发的超时时间间隔。uPeriod 定时器循环周期，若为0，则该定时器只运行一次。
LPTIMERNODE create_timer(void (*timerFn)(void*), void *pParam, uint32_t uDueTime, uint32_t uPeriod);

// 删除定时器
int32_t delete_timer(LPTIMERNODE lpTimer);

#endif
