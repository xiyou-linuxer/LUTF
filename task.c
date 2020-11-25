#include "task.h"
#include "stdint.h"
#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static tid_t tid = 0;   //tid递增

struct task_struct* main_task;   //主任务tcb
struct task_struct* current_task;   //记录当前任务
struct list task_ready_list;
struct list task_all_list;

static struct list_elem* task_tag;   //保存队列中的任务节点

/**
 * running_task -   获取当前任务的tcb
 * **/
struct task_struct* running_task()
{

}

/**
 * init_task - 初始化任务基本信息
 * **/
void init_task(struct task_struct* ptask, char* name, int prio)
{
    memset(ptask, 0, sizeof(*ptask));
    ptask->tid = tid++;
    strcpy(ptask->name, name);

    if(ptask == main_task) {
        ptask->status = TASK_RUNNING;
        current_task = ptask;
    } else {
        ptask->status = TASK_READY;
    }

    ptask->priority = prio;
    ptask->ticks = prio;
    ptask->elapsed_ticks = 0;

    ptask->stack_magic = 0x19991120;
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
    assert(!elem_find(&task_all_list, &main_task->all_list__tag));
    list_append(&task_all_list, &main_task->all_list__tag);
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
    
    //将当前main函数创建为任务
    make_main_task();

    printf("task_init done!\n");
}
