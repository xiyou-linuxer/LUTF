#include "task.h"
#include "analog_interrupt.h"
// #include <stdio.h>

int main()
{
    task_init();
    print_task_info(current_task);
    interrupt_enable();
    return 0;
}
