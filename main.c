#include "task.h"
#include "analog_interrupt.h"
#include <stdio.h>

void test(void)
{
    printf("AAAAAAAA\n");
}

int main()
{
    task_init();
    print_task_info(current_task);
    task_start("test", 31, test, NULL);
    struct task_struct* ptask = tid2task(1);
    print_task_info(ptask);
    interrupt_enable();
    return 0;
}
