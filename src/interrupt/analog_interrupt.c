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

/**
 * signal_headler - 模拟中断核心
 * **/
static void signal_headler(int signal_num)
{
	unsigned long a = 0;
	interrupt_timer_handler(&a);
}

/**
 * interrupt_init - 中断启动
 * **/
void interrupt_init()
{
  /* 模拟中断 */
	signal(SIGALRM, signal_headler);
  /* 10毫秒将会定时器SIGALRM就会唤醒一次中断 */
	if(set_ticker(10) == -1) {
    clock_granularity = 10;
		panic("set_ticker failed.");
	}
}

/**
 * interrupt_enable - 中断使能
 * **/
void interrupt_enable()
{
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);
}

/**
 * interrupt_disable - shield interrupt
 * **/
void interrupt_disable()
{
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}