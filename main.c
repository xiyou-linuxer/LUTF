#include "task.h"
#include "console.h"
#include "init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


void test(void* args)
{
    char* str = args;
    char s[20];
    snprintf(s,20,"@%d@\n",rand());
    while(1) {
        sleep(1);
        console_put_str(str);
        console_put_str(s);
    }}

void test1(void* args)
{
    char* str = args;
    char s[20];
    snprintf(s,20,"#%d#\n",rand());
    while(1) {
        sleep(1);
        console_put_str(str);
        console_put_str(s);
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
        console_put_str("rrr\n");
  }
}

int main()
{
    init();

    task_start("tast1", 31, test, "argA ");
    task_start("tast2", 31, test2, "test2");
    task_start("tast3", 31, test3, "test3");

    for(int i = 0; i < 10; i++) {
        task_start("abc", 31, test1, "a ");
    }
    while(1) {
        sleep(1);
        console_put_str("maiN ");
    }
    return 0;
}
