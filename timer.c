#include "timer.h"
#include <stdio.h>
#include "stdint.h"
#include "task.h"
#include "assert.h"

uint64_t ticks;

/**
 * interrupt_timer_handler - 时钟中断处理函数
 * **/
void interrupt_timer_handler(void)
{
    assert(current_task->stack_magic == 0x19991120);

    current_task->elapsed_ticks++;   //记录此任务占用cpu的时间
    ticks++;   //第一次处理时间中断后至今的滴答数，总滴答数
    // printf("!!!!!!!!!!!!\n");

    if(current_task->ticks == 0) {
        schedule();
    } else {
        current_task->ticks--;
    }
//    ticks++;
//    printf("ticks = %d\n", ticks);
}
