#include "timer.h"
#include "list.h"
#include "stdint.h"
#include "task.h"
#include "assert.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

uint64_t ticks;
int clock_granularity = 10;
LPTIMERMANAGER timer_manager;

static uint32_t get_jiffies(void)
{
    struct timespec ts;  // 精确到纳秒（10 的 -9 次方秒）
    // 使用 clock_gettime 函数时，有些系统需连接 rt 库，加 -lrt 参数，有些不需要连接 rt 库
    clock_gettime(CLOCK_MONOTONIC, &ts);  // 获取时间。其中，CLOCK_MONOTONIC 表示从系统启动这一刻起开始计时,不受系统时间被用户改变的影响
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);  // 返回毫秒时间
}

// 将双向循环链表的新结点插入到结点 pPrev 和 pNext 之间
static void list_timer_insert(struct LIST_TIMER *pNew, struct LIST_TIMER *pPrev, struct LIST_TIMER *pNext)
{
    pNext->pPrev = pNew;
    pNew->pNext = pNext;
    pNew->pPrev = pPrev;
    pPrev->pNext = pNew;
}

static void list_timer_insert_head(struct LIST_TIMER *pNew, struct LIST_TIMER *pHead)
{
    list_timer_insert(pNew, pHead, pHead->pNext);
}

static void list_timer_insert_tail(struct LIST_TIMER *pNew, struct LIST_TIMER *pHead)
{
    list_timer_insert(pNew, pHead->pPrev, pHead);
}

// 使用新结点 pNew 替换 pOld 在双向循环链表中的位置。如果双向链表中仅有一个结点 pOld，使用 pNew 替换后，同样，仅有一个结点 pNew
static void list_timer_replace(struct LIST_TIMER *pOld, struct LIST_TIMER *pNew)
{
    pNew->pNext = pOld->pNext;
    pNew->pNext->pPrev = pNew;
    pNew->pPrev = pOld->pPrev;
    pNew->pPrev->pNext = pNew;
}

// 使用新结点 pNew 替换 pOld 在双向循环链表中的位置。
static void list_timer_replace_init(struct LIST_TIMER *pOld, struct LIST_TIMER *pNew)
{
    list_timer_replace(pOld, pNew);
    // 使用 pNew 替换 pOld 在双向循环链表中的位置后，pOld 结点从链表中独立出来了，所以要让 pOld 指向自己
    pOld->pNext = pOld;
    pOld->pPrev = pOld;
}

// 初始化时间轮中的所有 tick。初始化后，每个 tick 中的双向链表只有一个头结点
static void init_array_list_timer(struct LIST_TIMER *arrListTimer, uint32_t nSize)
{
    uint32_t i;
    for(i=0; i<nSize; i++)
    {
        arrListTimer[i].pPrev = &arrListTimer[i];
        arrListTimer[i].pNext = &arrListTimer[i];
    }
}

static void delete_array_list_timer(struct LIST_TIMER *arrListTimer, uint32_t uSize)
{
    struct LIST_TIMER listTmr, *pListTimer;
    struct TIMER_NODE *pTmr;
    uint32_t idx;

    for(idx=0; idx<uSize; idx++)
    {
        list_timer_replace_init(&arrListTimer[idx], &listTmr);
        pListTimer = listTmr.pNext;
        while(pListTimer != &listTmr)
        {
            pTmr = (struct TIMER_NODE *)((char *)pListTimer - offsetof(struct TIMER_NODE, ltTimer));
            pListTimer = pListTimer->pNext;
            free(pTmr);
        }
    }
}

// 根据计时器的结束时间计算所属时间轮、在该时间轮上的 tick、然后将新计时器结点插入到该 tick 的双向循环链表的尾部
static void add_timer(LPTIMERNODE pTmr)
{
    struct LIST_TIMER *pHead;
    uint32_t i, uDueTime, uExpires;

    uExpires = pTmr->uExpires; // 定时器到期的时刻
    uDueTime = uExpires - timer_manager->uJiffies;
    uDueTime = uDueTime / clock_granularity; 
    if (uDueTime < TVR_SIZE)   // idx < 256 (2的8次方)
    {
        i = uExpires & TVR_MASK; // expires & 255
        pHead = &timer_manager->arrListTimer1[i];
    }
    else if (uDueTime < 1 << (TVR_BITS + TVN_BITS)) // idx < 16384 (2的14次方)
    {
        i = (uExpires >> TVR_BITS) & TVN_MASK;      // i = (expires>>8) & 63
        pHead = &timer_manager->arrListTimer2[i];
    }
    else if (uDueTime < 1 << (TVR_BITS + 2 * TVN_BITS)) // idx < 1048576 (2的20次方)
    {
        i = (uExpires >> (TVR_BITS + TVN_BITS)) & TVN_MASK; // i = (expires>>14) & 63
        pHead = &timer_manager->arrListTimer3[i];
    }
    else if (uDueTime < 1 << (TVR_BITS + 3 * TVN_BITS)) // idx < 67108864 (2的26次方)
    {
        i = (uExpires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK; // i = (expires>>20) & 63
        pHead = &timer_manager->arrListTimer4[i];
    }
    else if ((signed long) uDueTime < 0)
    {
        /*
         * Can happen if you add a timer with expires == jiffies,
         * or you set a timer to go off in the past
         */
        pHead = &timer_manager->arrListTimer1[(timer_manager->uJiffies & TVR_MASK)];
    }
    else
    {
        /* If the timeout is larger than 0xffffffff on 64-bit
         * architectures then we use the maximum timeout:
         */
        if (uDueTime > 0xffffffffUL)
        {
            uDueTime = 0xffffffffUL;
            uExpires = uDueTime + timer_manager->uJiffies;
        }
        i = (uExpires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK; // i = (expires>>26) & 63
        pHead = &timer_manager->arrListTimer5[i];
    }
    list_timer_insert_tail(&pTmr->ltTimer, pHead);
}

// 遍历时间轮 arrlistTimer 的双向循环链表，将其中的计时器根据到期时间加入到指定的时间轮中
static uint32_t cascade_timer(LPTIMERMANAGER timer_manager, struct LIST_TIMER *arrListTimer, uint32_t idx)
{
    struct LIST_TIMER listTmr, *pListTimer;
    struct TIMER_NODE *pTmr;

    list_timer_replace_init(&arrListTimer[idx], &listTmr);
    pListTimer = listTmr.pNext;
    // 遍历双向循环链表，添加定时器
    while(pListTimer != &listTmr)
    {
        // 根据结构体 struct TIMER_NODE 的成员 ltTimer 的指针地址和该成员在结构体中的便宜量，计算结构体 struct TIMER_NODE 的地址
        pTmr = (struct TIMER_NODE *)((char *)pListTimer - offsetof(struct TIMER_NODE, ltTimer));
        pListTimer = pListTimer->pNext;
        add_timer(pTmr);
    }
    return idx;
}

static void run_timer()
{
#define INDEX(N) ((timer_manager->uJiffies >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)
    uint32_t idx, uJiffies;
    struct LIST_TIMER listTmrExpire, *pListTmrExpire;
    struct TIMER_NODE *pTmr;

    if(NULL == timer_manager)
        return;
    uJiffies = get_jiffies();
    lock_acquire(&timer_manager->lock);
    while(TIME_AFTER_EQ(uJiffies, timer_manager->uJiffies))
    {
        // unint32 共 32bit，idx 为当前时间的低 8bit，INDEX(0) 为次 6bit，INDEX(1) 为再次 6bit，INDEX(2) 为再次 6bit，INDEX(3) 为高 6bit
        // 如果 1 级时间轮的 256 毫秒走完了，会遍历把 2 级时间轮中的计时器，将其中的计时器根据到期时间加入到指定时间轮。这样 1 级时间轮中就有计时器了。
        //  如果 1、2 级时间轮的 256*64 毫秒都走完了，会遍历 3 级时间轮，将其中的计时器加入到指定时间轮。这样 1 级和 2 级时间轮中就有计时器了。
        //   如果 1、2、3 级时间轮的 256*64*64 毫秒都走完了，...
        //    如果 1、2、3、4 级时间轮的 256*64*64*64 毫秒都走完了，...
        idx = timer_manager->uJiffies & TVR_MASK;
        if (!idx &&
                (!cascade_timer(timer_manager, timer_manager->arrListTimer2, INDEX(0))) &&
                (!cascade_timer(timer_manager, timer_manager->arrListTimer3, INDEX(1))) &&
                !cascade_timer(timer_manager, timer_manager->arrListTimer4, INDEX(2)))
            cascade_timer(timer_manager, timer_manager->arrListTimer5, INDEX(3));
        pListTmrExpire = &listTmrExpire;
        // 新结点 pListTmrExpire 替换 arrListTimer1[idx] 后，双向循环链表 arrListTimer1[idx] 就只有它自己一个结点了。pListTmrExpire 成为双向循环链表的入口
        list_timer_replace_init(&timer_manager->arrListTimer1[idx], pListTmrExpire);
        // 遍历时间轮 arrListTimer1 的双向循环链表，执行该链表所有定时器的回调函数
        pListTmrExpire = pListTmrExpire->pNext;
        while(pListTmrExpire != &listTmrExpire)
        {
            pTmr = (struct TIMER_NODE *)((char *)pListTmrExpire - offsetof(struct TIMER_NODE, ltTimer));
            pListTmrExpire = pListTmrExpire->pNext;
            pTmr->timerFn(pTmr->pParam);
            //
            if(pTmr->uPeriod != 0)
            {
                pTmr->uExpires = timer_manager->uJiffies + pTmr->uPeriod;
                add_timer(pTmr);
            }
            else free(pTmr);
        }
        timer_manager->uJiffies++;
    }
    lock_release(&timer_manager->lock);
}

// 测试函数
void timer_test(void *pParam)
{
    LPTIMERMANAGER pMgr;
    pMgr = (LPTIMERMANAGER)pParam;
    printf("Timer expire! Jiffies: %u\n", pMgr->uJiffies);
}

// 创建定时器管理器
void timer_init(void)
{
    timer_manager = (LPTIMERMANAGER)malloc(sizeof(TIMERMANAGER));
    if(timer_manager != NULL)
    {
        timer_manager->uExitFlag = 0;
        lock_init(&timer_manager->lock);
        timer_manager->uJiffies = get_jiffies();
        init_array_list_timer(timer_manager->arrListTimer1, sizeof(timer_manager->arrListTimer1)/sizeof(timer_manager->arrListTimer1[0]));
        init_array_list_timer(timer_manager->arrListTimer2, sizeof(timer_manager->arrListTimer2)/sizeof(timer_manager->arrListTimer2[0]));
        init_array_list_timer(timer_manager->arrListTimer3, sizeof(timer_manager->arrListTimer3)/sizeof(timer_manager->arrListTimer3[0]));
        init_array_list_timer(timer_manager->arrListTimer4, sizeof(timer_manager->arrListTimer4)/sizeof(timer_manager->arrListTimer4[0]));
        init_array_list_timer(timer_manager->arrListTimer5, sizeof(timer_manager->arrListTimer5)/sizeof(timer_manager->arrListTimer5[0]));
    }
    // timer_test
    LPTIMERNODE pTn = create_timer(timer_test, timer_manager, 1000, 0);
    return ;
}

// 删除定时器管理器
void destroy_timer_manager()
{
    if(NULL == timer_manager)
        return;
    timer_manager->uExitFlag = 1;

    delete_array_list_timer(timer_manager->arrListTimer1, sizeof(timer_manager->arrListTimer1)/sizeof(timer_manager->arrListTimer1[0]));
    delete_array_list_timer(timer_manager->arrListTimer2, sizeof(timer_manager->arrListTimer2)/sizeof(timer_manager->arrListTimer2[0]));
    delete_array_list_timer(timer_manager->arrListTimer3, sizeof(timer_manager->arrListTimer3)/sizeof(timer_manager->arrListTimer3[0]));
    delete_array_list_timer(timer_manager->arrListTimer4, sizeof(timer_manager->arrListTimer4)/sizeof(timer_manager->arrListTimer4[0]));
    delete_array_list_timer(timer_manager->arrListTimer5, sizeof(timer_manager->arrListTimer5)/sizeof(timer_manager->arrListTimer5[0]));
    free(timer_manager);
}

// 创建一个定时器。fnTimer 回调函数地址。pParam 回调函数的参数。uDueTime 首次触发的超时时间间隔。uPeriod 定时器循环周期，若为0，则该定时器只运行一次。
LPTIMERNODE create_timer(void (*timerFn)(void*), void *pParam, uint32_t uDueTime, uint32_t uPeriod)
{
    LPTIMERNODE pTmr = NULL;
    if(NULL == timerFn || NULL == timer_manager)
        return NULL;
    pTmr = (LPTIMERNODE)malloc(sizeof(TIMERNODE));
    if(pTmr != NULL)
    {
        pTmr->uPeriod = uPeriod;
        pTmr->timerFn = timerFn;
        pTmr->pParam = pParam;

        lock_acquire(&timer_manager->lock);
        pTmr->uExpires = timer_manager->uJiffies + uDueTime;
        add_timer(pTmr);
        lock_release(&timer_manager->lock);
    }
    return pTmr;
}

//删除定时器
int32_t delete_timer(LPTIMERNODE lpTimer)
{
    struct LIST_TIMER *pListTmr;
    if(NULL != timer_manager && NULL != lpTimer)
    {
        lock_acquire(&timer_manager->lock);
        // 变成开关中断,没证明可用
        pListTmr = &lpTimer->ltTimer;
        pListTmr->pPrev->pNext = pListTmr->pNext;
        pListTmr->pNext->pPrev = pListTmr->pPrev;
        free(lpTimer);
        lock_release(&timer_manager->lock);
        return 0;
    }
    else
        return -1;
}

/**
 * interrupt_timer_handler - 时钟中断处理函数
 * **/
void interrupt_timer_handler(unsigned long* a)
{
    assert(current_task->stack_magic == 0x19991120);
    current_task->elapsed_ticks++;   //记录此任务占用cpu的时间
    ticks++;   //第一次处理时间中断后至今的滴答数，总滴答数
    /* 时间片用尽执行 schedule() */
    if(current_task->ticks == 0) {
        // 调度前先走定时器模块
        if(NULL != timer_manager) {
            if(!timer_manager->uExitFlag) {
                run_timer();
            } else {
                destroy_timer_manager();
            }
        }
        schedule(a);
    } else {
        current_task->ticks--;
    }
}
