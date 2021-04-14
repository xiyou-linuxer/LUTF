#include "analog_interrupt.h"
#include "timer.h"
#include "stdint.h"
#include "assert.h"
#include "set_ticker.h"
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void signal_headler(int signal_num)
{
	unsigned long a = 0;
	// printf("ZZZZZZZZZZZZZZZZ\n");
	interrupt_timer_handler(&a);
	// printf("ticks = %d\n", ticks);
}

/**
 * interrupt_enable - 中断使能
 * **/
void interrupt_enable()
{
    /* 模拟中断 */
	signal(SIGALRM, signal_headler);
	if(set_ticker(10) == -1) {
		perror("set_ticker");
	}
}

/**
 * interrupt_disable - shield interrupt
 * **/
void interrupt_disable()
{
	signal(SIGALRM, SIG_IGN);
}