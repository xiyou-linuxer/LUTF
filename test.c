#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>

#include "set_ticker.h"
#include "stdint.h"
#include "assert.h"

// int set_ticker(int n_msecs);

uint64_t ticks = 0;

void signal_headler(int signal_num)
{
	printf("ticks = %lld\n", ticks++);
}

int main()
{
    /* 模拟中断 */
    printf("!!!!\n");
    sleep(3);
    printf("AAAA\n");
	signal(SIGALRM, signal_headler);
	if(set_ticker(1000) == -1) {
		perror("set_ticker");
	} else {
		while(1) {
			pause();
		}
	}

    /*jmp_buf env;
    int i;
    
    i = setjmp(env);

    printf("i = %d\n", i);
    if(i != 0) {
        exit(1);
    }

    longjmp(env, 2);*/

	return 0;
}
