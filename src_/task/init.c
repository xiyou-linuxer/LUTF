#include "init.h"
#include "task.h"
#include "console.h"
#include "ioqueue.h"

void init()
{
    console_init();
    task_init();
    return;
}
