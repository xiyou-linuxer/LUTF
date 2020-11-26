#include "task.h"
#include "analog_interrupt.h"
#include <stdio.h>
#include <unistd.h>

void test(void* args)
{
    while(1) {
        sleep(1);
        printf("AAAAAAAA\n");
        // pause();
    }
}

void test1(void* args)
{
    while(1) {
        // msleep(100);
        printf("BBBBBBBB\n");
        // pause();
    }
}

int main()
{
    task_init();
    print_task_info(current_task);
    task_start("test", 31, test, NULL);
    task_start("tast1", 31, test1, NULL);
    struct task_struct* ptask = tid2task(1);
    print_task_info(ptask);
    ptask = tid2task(2);
    print_task_info(ptask);
    interrupt_enable();
    return 0;
}
