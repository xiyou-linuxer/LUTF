#ifndef __TIMER_H
#define __TIMER_H

#include "stdint.h"

extern uint64_t ticks;

/**
 * interrupt_timer_handler - 时钟中断处理函数
 * **/
void interrupt_timer_handler(void);

#endif
