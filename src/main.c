#include "task.h"
#include "console.h"
#include "init.h"
#include <stdio.h>
#include <unistd.h>

void test1(void* args)
{
    char* str = (char*)args;
    while(1) {
        sleep(1);
        console_put_str(str);
    }
}

int main()
{
    init();
    task_start("tast1", 31, test1, (void*)"argB ");
    for(int i = 0; i < 10; i++) {
        task_start("abc", 31, test1, (void*)"a ");
    }
    while(1) {
        sleep(1);
        console_put_str("maiN ");
    }
    return 0;
}