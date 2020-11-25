#include "analog_interrupt.h"
#include "stdint.h"
#include "assert.h"
#include "set_ticker.h"
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

uint64_t ticks = 0;

static void signal_headler(int signal_num)
{
	printf("ticks = %lld\n", ticks++);
}

/**
 * interrupt_enable - 中断使能
 * **/
void interrupt_enable()
{
    /* 模拟中断 */
	signal(SIGALRM, signal_headler);
	if(set_ticker(1000) == -1) {
		perror("set_ticker");
	} else {
		while(1) {
			pause();
		}
	}
}
