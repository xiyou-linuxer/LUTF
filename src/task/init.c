#include "init.h"
#include "task.h"
#include "timer.h"
#include "console.h"
#include "ioqueue.h"
#include "analog_interrupt.h"

void init()
{
    console_init();
    task_init();
    timer_init();
    interrupt_init();
    return;
}
