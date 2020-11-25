#ifndef __TASK_H
#define __TASK_H

#include "stdint.h"
#include "list.h"
#include <setjmp.h>

typedef int16_t tid_t;

extern struct list task_ready_list;
extern struct list task_all_list;
extern struct task_struct* current_task;

enum task_status
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED
};

struct task_struct
{
    jmp_buf env;
    tid_t tid;   //任务id
    enum task_status status;   //任务状态
    char name[32];   //任务名
    uint8_t priority;   //任务优先级，通过优先级设置时间片
    uint8_t ticks;   //每次处理器上执行的时间嘀嗒数，任务的时间片
    
    uint32_t elapsed_ticks;   //任务从开始到结束的总滴答数

    struct list_elem general_tag;
    struct list_elem all_list__tag;

    uint32_t stack_magic;   //魔数
};

/** 
 * task_init - 初始化任务 
 * **/
void task_init(void);

/**
 * init_task - 初始化任务基本信息
 * **/
void init_task(struct task_struct* ptask, char* name, int prio);

/**
 * print_task_info - 打印task信息
 * **/
void print_task_info(struct task_struct* ptask);

#endif
