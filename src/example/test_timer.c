#include "../include/task.h"
#include "../include/timer.h"
#include "../include/init.h"
#include <stdio.h>
#include <unistd.h>

void test(void* args)
{
    char* str = args;
    while(1) {
        sleep(1);
        console_put_str(str);
    }
}

// 测试函数
void timer_test(void *pParam)
{
    LPTIMERMANAGER pMgr;
    pMgr = (LPTIMERMANAGER)pParam;
    printf("Timer expire! Jiffies: %u\n", pMgr->uJiffies);
}

int main()
{
    init();
    // timer_test
    LPTIMERNODE pTn = create_timer(timer_test, timer_manager, 10000, 10000);
    for(int i = 0; i < 100; i++) {
        task_start("abc", 31, test, "a ");
    }
    while(1) {
        sleep(1);
        console_put_str("maiN ");
    }
    return 0;
}
