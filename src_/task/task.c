#include "task.h"
#include "stdint.h"
#include "assert.h"
#include "analog_interrupt.h"
#include "bitmap.h"
#include "debug.h"
#include "sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <ucontext.h>

#define TASK_STACK_SIZE (1024 * 4)   //任务栈的大小
#define CONTEXT_OFFSET  72

#define JB_RBX   0
#define JB_RBP   1
#define JB_R12   2
#define JB_R13   3
#define JB_R14   4
#define JB_R15   5
#define JB_RSP   6
#define JB_PC    7
#define JB_SIZE  (8 * 8)

// static tid_t tid = 0;   //tid递增

//tid位图，最大支持1024个tid
uint8_t tid_bitmap_bits[1250000] = {0};

struct tid_pool
{
    struct bitmap tid_bitmap;   //tid位图
    uint32_t tid_start;   //起始tid
    struct lock tid_lock;
}tid_pool;

struct task_struct* main_task;   //主任务tcb
struct task_struct* current_task;   //记录当前任务
struct list task_ready_list;
struct list task_all_list;

static struct list_elem* task_tag;   //保存队列中的任务节点
static void died_task_schedule();
static void block_task_schedule();

extern void context_set(struct sigcontext* context);
extern void context_swap(struct sigcontext* c_context, struct sigcontext* n_context);

/**
 * task_entrance - 执行任务函数function(func_arg)
 * @function: 任务处理函数
 * @func_arg: 任务参数
 * **/
static void task_entrance(task_func* function, void* func_arg)
{
    function(func_arg);
    task_exit(current_task);
}

/**
 * tid_pool_init - 初始化tid池
 * **/
static void tid_pool_init(void)
{
    tid_pool.tid_start = 1;
    tid_pool.tid_bitmap.bits = tid_bitmap_bits;
    tid_pool.tid_bitmap.btmp_bytes_len = 128;
    bitmap_init(&tid_pool.tid_bitmap);
    lock_init(&tid_pool.tid_lock);
}

/**
 * 分配tid
 * **/
static tid_t allocate_tid(void)
{
    lock_acquire(&tid_pool.tid_lock);
    int32_t bit_idx = bitmap_scan(&tid_pool.tid_bitmap, 1);
    bitmap_set(&tid_pool.tid_bitmap, bit_idx, 1);
    lock_release(&tid_pool.tid_lock);
    return (bit_idx + tid_pool.tid_start);
}

/**
 * release_tid - 释放tid
 * @tid: 要释放的tid
 * **/
void release_tid(tid_t tid)
{
    lock_acquire(&tid_pool.tid_lock);
    int32_t bit_idx = tid - tid_pool.tid_start;
    bitmap_set(&tid_pool.tid_bitmap, bit_idx, 0);
    lock_release(&tid_pool.tid_lock);
}

/**
 * task_exit - 任务结束
 * @task: 结束的任务的task_struct
 * **/
void task_exit(struct task_struct* task)
{
    //关闭中断
    interrupt_disable();

    task->status = TASK_DIED;

    //在就绪队列中删除
    if(elem_find(&task_ready_list, &task->general_tag)) {
        list_remove(&task->general_tag);
    }
    
    //在全部任务队列中删除
    list_remove(&task->all_list_tag);

    if(task != main_task) {
        free(task->stack_min_addr);
        free(task);
    }

    //归还tid
    release_tid(task->tid);

    //打开中断
    interrupt_enable();

    //上下文转换为next的上下文
    died_task_schedule();
}

/**
 * tid_check - 对比任务的tid
 * @pelem: 要比较的任务节点
 * @tid: 要对比的tid
 * **/
static bool tid_check(struct list_elem* pelem, int32_t tid)
{
    struct task_struct* ptask = elem2entry(struct task_struct, all_list_tag, pelem);
    if(ptask->tid == tid) {
        return true;
    }
    return false;
}

/**
 * init_task - 初始化任务基本信息
 * **/
void init_task(struct task_struct* ptask, char* name, int prio)
{
    memset(ptask, 0, sizeof(*ptask));
    // ptask->tid = tid++;
    // ptask->tid = allocate_tid();
    strcpy(ptask->name, name);

    if(ptask == main_task) {
        ptask->status = TASK_RUNNING;
        ptask->first = false;
        current_task = ptask;
    } else {
        ptask->status = TASK_READY;
        ptask->first = true;
    }
    ptask->tid = allocate_tid();

    //task_stack指向栈顶
    // uint8_t* stack_min_addr = (uint8_t*)malloc(TASK_STACK_SIZE);
    ptask->stack_min_addr = (uint8_t*)malloc(TASK_STACK_SIZE);
    // ptask->stack_min_addr = stack_min_addr;
    // ptask->task_stack = (uint64_t*)(stack_min_addr + TASK_STACK_SIZE);
    ptask->task_stack = (uint64_t*)(ptask->stack_min_addr + TASK_STACK_SIZE);

    ptask->priority = prio;
    ptask->ticks = prio;
    ptask->elapsed_ticks = 0;
    ptask->stack_magic = 0x19991120;
}

/**
 * task_create - 创建（初始化）一个任务
 * @ptask: 任务结构体指针
 * @function: 任务的功能函数
 * @func_arg: 任务功能函数的参数
 * **/
/*
static void task_create(struct task_struct* ptask, task_func function, void* func_arg)
{
    //init sigjmp_buf;
    sigsetjmp(ptask->env, 1);

    //预留任务栈空间
    ptask->task_stack -= sizeof(struct task_stack);
    struct task_stack* ptask_stack = (struct task_stack*)ptask->task_stack;
    ptask_stack->rsp = (uint64_t)ptask_stack;
    ptask_stack->rip = (uint64_t)function;
    ptask_stack->func_arg = func_arg;

    ptask_stack->rbx = ptask_stack->rbp = ptask_stack->r12 = \
    ptask_stack->r13 = ptask_stack->r14 = ptask_stack->r15 = 0;
    // ptask->function = function;
    ptask->func_args = func_arg;
    
    //构造sigjmp_buf
    __jmp_buf* jmp_buf = (__jmp_buf*)&(ptask->env->__jmpbuf);
    **(jmp_buf + JB_RBX*8) = 0;
    **(jmp_buf + JB_RBP*8) = 0;
    **(jmp_buf + JB_R12*8) = 0;
    **(jmp_buf + JB_R13*8) = 0;
    **(jmp_buf + JB_R14*8) = 0;
    **(jmp_buf + JB_R15*8) = 0;
    **(jmp_buf + JB_RSP*8) = ptask_stack->rsp;
    **(jmp_buf + JB_PC *8) = ptask_stack->rip;
    printf("fc_addr = %p\n", function);
    printf("ip_addr = %p\n", **(jmp_buf + JB_PC*8));
}
*/

static void task_create(struct task_struct* ptask, task_func function, void* func_arg)
{
    printf("func_arg = 0x%lx\n", func_arg);
    //init sigjmp_buf;
    
    //init stack space
    // *(ptask->task_stack - 8) =  func_arg;
    // *(ptask->task_stack - 16) = function;
    // *(ptask->task_stack - 16) = 0x0;
    // ptask->task_stack -= 8 * 2;

    //create task's context
    memset(&ptask->context, 0, sizeof(ptask->context));
    // ptask->context.rsi = 0x4;
    // ptask->context.rdi = 0x4c315e80;
    // ptask->context.rbx = 0xed3bf400;
    // ptask->context.rbp = 0xffffff80;

    //ss should be 0x2b
    ptask->context.__pad0 = 0x2b;
    ptask->context.rsp = ptask->context.rbp =  ptask->task_stack;
    ptask->context.cs = 0x33;
    ptask->context.rip = task_entrance;
    ptask->context.rdi = function;
    ptask->context.rsi = func_arg;

    // ptask->function = function;
    // ptask->func_args = func_arg;
}

/**
 * task_start - 创建一个优先级为prio，名字为name的任务
 * @name: 任务名
 * @prio: 任务优先级
 * @func: 任务处理函数
 * @func_arg: 任务参数
 * **/
struct task_struct* task_start(char* name, int prio, task_func function, void* func_arg)
{
    interrupt_disable();
    struct task_struct* task = (struct task_struct*)malloc(sizeof(struct task_struct));

    init_task(task, name, prio);
    task_create(task, function, func_arg);

    //之前不再队列中
    assert(!elem_find(&task_ready_list, &task->general_tag));
    //加入就绪任务队列
    list_append(&task_ready_list, &task->general_tag);

    //之前不再全部任务队列中
    assert(!elem_find(&task_all_list, &task->all_list_tag));
    //加入到全部任务队列
    list_append(&task_all_list, &task->all_list_tag);

    interrupt_enable();
    return task;
}

/**
 * make_main_task - 将main函数变为任务
 * **/
static void make_main_task(void)
{
    //main任务分配task_struct结构体
    main_task = (struct task_struct*)malloc(sizeof(struct task_struct));
    
    //初始化main任务的task_struct
    init_task(main_task, "main", 31);

    //main函数是当前任务，当前还不再task_ready_list中，只加入task_all_list
    assert(!elem_find(&task_all_list, &main_task->all_list_tag));
    list_append(&task_all_list, &main_task->all_list_tag);
}

/**
 * tid2task - 根据tid获得task_struct
 * @tid: 任务的tid
 * **/
struct task_struct* tid2task(tid_t tid)
{
    // struct list_elem* pelem = task_all_list.head.next;
    // struct task_struct* ptask = NULL;
    // while(pelem != &task_all_list.tail) {
    //     ptask = elem2entry(struct task_struct, all_list_tag, pelem);
    //     if(ptask->tid == tid) {
    //         break;
    //     }
    //     ptask = NULL;
    //     pelem = pelem->next;
    // }
    struct list_elem* pelem = list_traversal(&task_all_list, tid_check, tid);
    if(pelem == NULL) {
        return NULL;
    }
    struct task_struct* ptask = elem2entry(struct task_struct, all_list_tag, pelem);

    return ptask;
}

/**
 * task_block - 当前任务阻塞自己，标志其状态为status
 * @status: 转变为该状态
 * **/
void task_block(enum task_status status)
{
    //status取值为BLOCKED,WAITTING,HANGING，这三种状态不会被调度
    assert(((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANGING)));
    current_task->status = status;   //置其状态为status
    block_task_schedule();   //将当前线程换下处理器
}

/**
 * task_unblock - 将任务ptask解除阻塞
 * @ptask: 要解除阻塞的任务结构体指针
 * **/
void task_unblock(struct task_struct* ptask)
{
    interrupt_disable();
    assert(((ptask->status == TASK_BLOCKED) || (ptask->status == TASK_WAITING) || (ptask->status == TASK_HANGING)));
    if(ptask->status != TASK_READY) {
        assert(!elem_find(&task_ready_list, &ptask->general_tag));
        if(elem_find(&task_ready_list, &ptask->general_tag)) {
            PANIC("thread_unblock: block thread in ready list\n");
        }
        list_push(&task_ready_list, &ptask->general_tag);   //放到队列的最前面，使其尽快得到调度
        ptask->status = TASK_READY;
    }
    interrupt_enable();
}

/**
 * print_task_info - 打印task信息
 * **/
void print_task_info(struct task_struct* ptask)
{
    printf("tid = %d\n", ptask->tid);
    printf("name = %s\n", ptask->name);
    printf("priority = %d\n", ptask->priority);
    printf("stack_magic = %x\n", ptask->stack_magic);
}

/** 
 * task_init - 初始化任务 
 * **/
void task_init(void)
{
    printf("task_init start.\n");
    list_init(&task_ready_list);
    list_init(&task_all_list);
    tid_pool_init();
    
    //将当前main函数创建为任务
    make_main_task();

    printf("task_init done!\n");
}

/**
 * schedule - 任务调度(signal handle)
 * **/
void schedule(unsigned long* a)
{
    unsigned char* p;
    assert(!elem_find(&task_ready_list, &current_task->general_tag));
    list_append(&task_ready_list, &current_task->general_tag);
    current_task->ticks = current_task->priority;
    current_task->status = TASK_READY;

    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1);
    }

    assert(!list_empty(&task_ready_list));
    task_tag = NULL;
    task_tag = list_pop(&task_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;

    //调度
    p = (unsigned char*)((unsigned char*)a + CONTEXT_OFFSET);
    struct sigcontext* context = (struct sigcontext*)p;
    // printf("%x\n%x\n%x\n%x\n", context->rsi, context->rdi, context->rbx, context->rbp);
    // current_task->context = (struct sigcontext*)p;
    //save current task context
    // memcpy(p, &current_task->context, sizeof(struct sigcontext));
    memcpy(&current_task->context, p, sizeof(struct sigcontext));

    //running next task
    current_task = next;
    // memcpy(&current_task->context, p, sizeof(struct sigcontext));
    memcpy(p, &current_task->context, sizeof(struct sigcontext));

    // switch_to_next(next);   //保存当前，sigjmp_buf, |  siglongjmp();
}

/**
 * switch_to - 任务切换
 * **/
/*
static void switch_to_next(struct task_struct* next)
{
    //保存当前任务上下文
    int ret;
    ret = sigsetjmp(current_task->env, 1);   //保存当前上下文 i = 0
    if(ret != 0) {
        return;
    }
    
    //第一次执行的任务执行任务的任务函数
    // if(next->first == true) {
    //     next->first = false;
    //     current_task = next;
    //     // next->function(next->func_args);   //while(1) printf("AAAAAAAA\n");
    //     first_running(next->function, next->func_args);   //while(1) printf("AAAAAAAA\n");
    //     return;
    //     // return next->function(next->func_args);
    // }

    current_task = next;
    siglongjmp(next->env, 1);
}
*/

static void switch_to_next(struct task_struct* next)
{
    //保存当前任务上下文

    
    current_task = next;
    siglongjmp(next->env, 1);
}

/**
 * died_task_schedule - 死亡任务主动让出CPU
 * **/
static void died_task_schedule()
{
    //屏蔽信号，退出后要打开信号
    
    //获取下一个要调度的任务
    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1);
    }

    assert(!list_empty(&task_ready_list));
    task_tag = NULL;
    task_tag = list_pop(&task_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;

    //进行上下文切换，将上下文切换为要执行任务的上下文
    current_task = next;
    context_set(&current_task->context);
}

static void block_task_schedule()
{
    //获取下一个要调度的任务
    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1);
    }

    assert(!list_empty(&task_ready_list));
    task_tag = NULL;
    task_tag = list_pop(&task_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;

    //进行上下文切换，将上下文切换为要执行任务的上下文
    struct task_struct* temp = current_task;
    current_task = next;
    context_swap(&temp->context, &next->context);
}
