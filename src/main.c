#include "task.h"
#include "console.h"
#include "init.h"
#include <stdio.h>
#include <unistd.h>

void test(void* args)
{
    char* str = args;
    console_put_str(str);
}

void test1(void* args)
{
    char* str = args;
    while(1) {
        sleep(1);
        console_put_str(str);
    }
}

/* cpu密集 */
void test2(void* args)
{
  while(1);
}

/* io密集 */
void test3(void* args)
{
  while(1){
        sleep(1);
        console_put_str("rrr\n");
  }
}

int main()
{
    init();
    task_start("tast1", 31, test, "argA ");
    for(int i = 0; i < 100; i++) {
        task_start("abc", 31, test1, "a ");
    }
    while(1) {
        sleep(1);
        console_put_str("maiN ");
    }
    return 0;
}
