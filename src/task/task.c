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

//tid位图，最大支持n*8个tid
uint8_t tid_bitmap_bits[125100] = {0};

struct tid_pool
{
    struct bitmap tid_bitmap;   //tid位图
    uint32_t tid_start;   //起始tid
    struct lock tid_lock;
}tid_pool;

struct task_struct* main_task;   //主任务tcb
struct task_struct* current_task;   //记录当前任务
struct list task_ready_list; /* 就绪任务列表 */
struct list task_all_list; // 所有任务列表
struct list task_pool_list; // task 池,减少内存分配
int task_all_nums;          // task_ready_list 中的 task 数量
static struct list_elem* task_tag;   //保存队列中的任务节点
static void died_task_schedule();
static void block_task_schedule();

void context_set(struct sigcontext* context);
void get_reg(struct sigcontext* context);
void context_swap(struct sigcontext* c_context, struct sigcontext* n_context);

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
    tid_pool.tid_bitmap.btmp_bytes_len = 125100;
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
    // 关闭中断
    interrupt_disable();
    // 此处可以做优化
    task->status = TASK_DIED;

    //在就绪队列中删除
    if(elem_find(&task_ready_list, &task->general_tag)) {
        list_remove(&task->general_tag);
    }

    //在全部任务队列中删除
    list_remove(&task->all_list_tag);
    // 统计 task_all_list 上的任务数减少1
    task_all_nums--;

    /* 如果不是main任务，则释放所有的空间 */
    if(task != main_task) {
        // 做池化处理暂时不释放,当定时器可用之后在释放
        list_append(&task_pool_list,&task->pool_tag);
        // free(task->stack_min_addr);
        // free(task);
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
    ptask->sleep_millisecond = 0;
    ptask->is_hook = false;
    ptask->is_collaborative_schedule = false;
}

/**
 * task_create - 创建（初始化）一个任务
 * @ptask: 任务结构体指针
 * @function: 任务的功能函数
 * @func_arg: 任务功能函数的参数
 * **/
static void task_create(struct task_struct* ptask, task_func function, void* func_arg)
{
    memset(&ptask->context, 0, sizeof(ptask->context));
    /* 设置任务的栈帧 */
    ptask->context.rsp = ptask->context.rbp = (uint64_t)ptask->task_stack;
    get_reg(&ptask->context);
    /* 设置任务的 cs:ip */
    ptask->context.rip = (uint64_t)task_entrance;
    ptask->context.rdi = (uint64_t)function;
    ptask->context.rsi = (uint64_t)func_arg;
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
    struct task_struct* task;
    // 这里可以不去 malloc 先去池里面取
    if(!list_empty(&task_pool_list)) {
        // 去 task_pool 去取一个 task 
        task_tag = list_pop(&task_pool_list);
        task = elem2entry(struct task_struct, pool_tag, task_tag);
        assert(TASK_DIED != task->status);
    } else {
        // 如果池里面没有,就 malloc 一个
        task = (struct task_struct*)malloc(sizeof(struct task_struct));
    } 
    
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
    // 统计 task_all_list 中的 task 数量
    task_all_nums++;

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
    init_task(main_task, "main", 3);

    //main函数是当前任务，当前还不再task_ready_list中，只加入task_all_list
    assert(!elem_find(&task_all_list, &main_task->all_list_tag));
    list_append(&task_all_list, &main_task->all_list_tag);
    task_all_nums++; // 统计 task_all_list 数量
}

/**
 * tid2task - 根据tid获得task_struct
 * @tid: 任务的tid
 * **/
struct task_struct* tid2task(tid_t tid)
{
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
    /* 禁用中断 */
    interrupt_disable();
    assert(((ptask->status == TASK_BLOCKED) || (ptask->status == TASK_WAITING) || (ptask->status == TASK_HANGING)));
    if(ptask->status != TASK_READY) {
        assert(!elem_find(&task_ready_list, &ptask->general_tag));
        list_push(&task_ready_list, &ptask->general_tag);   //放到队列的最前面，使其尽快得到调度
        ptask->status = TASK_READY;
    }
    /* 启用中断 */
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
    list_init(&task_ready_list);
    list_init(&task_all_list);
    list_init(&task_pool_list);
    task_all_nums = 0;
    tid_pool_init();
    //将当前main函数创建为任务
    make_main_task();
}

/**
 * schedule - 任务调度(signal handle)
 * **/
void schedule(unsigned long* a)
{
    unsigned char* p;
    /* 保证在就绪队列中找不到task_ready_list */
    assert(!elem_find(&task_ready_list, &current_task->general_tag));
    /* 在就绪队列最后加上当前任务 */
    list_append(&task_ready_list, &current_task->general_tag);
    /* 设置当前任务的时间片 */
    current_task->ticks = current_task->priority;
    current_task->status = TASK_READY;

    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1){
            printf("nihao\n");
        };   // TODO 当main函数执行sleep的时候可能会卡死，因为hook后的sleep不会把current_task插入ready_list
    }

    /* 获取下一个任务 */
    task_tag = list_pop(&task_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;
    //从信号栈esp+72获得sigcontext的内容
    p = (unsigned char*)((unsigned char*)a + CONTEXT_OFFSET);
    struct sigcontext* context = (struct sigcontext*)p;
    /* 将当前任务的上下文保存到 current_task的上下文中 */
    memcpy(&current_task->context, context, sizeof(struct sigcontext));
    /* 将下个任务的上下文取出使用 */
    current_task = next;
    memcpy(context, &current_task->context, sizeof(struct sigcontext));
}

/**
 * collaborative_schedule - 协作式时的任务调度，与schedule的区别就是不会把current_task插入就绪队列
 * 而是等在超时时间结束以后再插入就绪队列。
 * **/
void collaborative_schedule(unsigned long* a)
{
    unsigned char* p;

    if(list_empty(&task_ready_list)) {
        while(1){
        }
    }

    /* 获取下一个任务 */
    task_tag = list_pop(&task_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;
    //从信号栈esp+72获得sigcontext的内容
    p = (unsigned char*)((unsigned char*)a + CONTEXT_OFFSET);
    struct sigcontext* context = (struct sigcontext*)p;
    /* 将当前任务的上下文保存到 current_task的上下文中 */
    memcpy(&current_task->context, context, sizeof(struct sigcontext));
    /* 将下个任务的上下文取出使用 */
    current_task = next;
    memcpy(context, &current_task->context, sizeof(struct sigcontext));
}

/**
 * died_task_schedule - 死亡任务主动让出CPU
 * **/
static void died_task_schedule()
{
    //屏蔽信号，退出后要打开信号
    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1);
    }
    //获取下一个要调度的任务
    task_tag = list_pop(&task_ready_list);
    /* 还是取出就绪队列下一个任务 */
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    next->status = TASK_RUNNING;
    //进行上下文切换，将上下文切换为要执行任务的上下文
    current_task = next;
    context_set(&current_task->context);
}

/**
 * block_task_schedule - 阻塞当前任务并切换到下一个要调度的任务执行
 * **/
static void block_task_schedule()
{
    /* 如果就绪列表是空的，那么说明无事可做了 */
    if(list_empty(&task_ready_list)) {
        printf("task_ready_list is empty!\n");
        while(1);
    }
    assert(!list_empty(&task_ready_list));
    task_tag = list_pop(&task_ready_list);
    /* 获得下一个任务 */
    struct task_struct* next = elem2entry(struct task_struct, general_tag, task_tag);
    /* 设置任务状态为RUNNING */
    next->status = TASK_RUNNING;
    /* 进行上下文切换，将上下文切换为要执行任务的上下文 */
    struct task_struct* temp = current_task;
    current_task = next;
    context_swap(&temp->context, &next->context);
}

/**
 * 判断当前任务是否希望被hook
 * **/
bool current_is_hook(){
   return current_task && current_task->is_hook; 
}
// 测试函数
void clean_dead_task(void)
{
    // 如果任务池为空
    if(list_empty(&task_pool_list)) { 
        return ;
    } else {
        struct task_struct *task;
        int clean_cnt = task_all_nums/8 > 16 ? 16 : task_all_nums/8;
        while(list_empty(&task_pool_list) || clean_cnt == 0){
            task_tag = list_pop(&task_pool_list);
            clean_cnt--;
            task = elem2entry(struct task_struct, pool_tag, task_tag);
            free(task->stack_min_addr);
            free(task);
        }
    }
    return ;
}