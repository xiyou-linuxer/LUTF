#include "task.h"
#include "analog_interrupt.h"
#include <stdio.h>
#include <unistd.h>

void test(void* args)
{
    while(1) {
        sleep(1);
        printf("AAAAAAAA\n");
        // task_exit(current_task);
        // pause();
    }
}

void test1(void* args)
{
    while(1) {
        sleep(1);
        printf("BBBBBBBB\n");
        // task_exit(current_task);
        // pause();
    }
}

int main()
{
    printf("sizeof(long int) = %ld\n", sizeof(long int));
    task_init();
    print_task_info(current_task);
    task_start("test", 31, test, NULL);
    task_start("tast1", 31, test1, NULL);
    struct task_struct* ptask = tid2task(1);
    print_task_info(ptask);
    ptask = tid2task(2);
    print_task_info(ptask);
    interrupt_enable();
    while(1) {
        sleep(1);
        printf("MMMMMMMM\n");
        // pause();
    }
    return 0;
}
