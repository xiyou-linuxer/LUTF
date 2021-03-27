#include "task.h"
#include "analog_interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "init.h"
#include <stdio.h>
#include <unistd.h>
#include <ucontext.h>

struct ioqueue buf;

void test(void* args)
{
    // char* str = args;
    int* n = args;
    while(1) {
        sleep(1);
        // printf("A, a = %d\n", a++);
        // printf("AAAAAAAAAAAA\n");
        // console_get_str();
        // console_put_str("hello\n");
        // char byte = ioq_getchar(&buf);
        // console_put_char(byte);
        // console_put_str(str);
        console_put_int(*n);
        // task_exit(current_task);
        // task_exit(current_task);
        // pause();
    }
}

void test1(void* args)
{
    char* str = args;
    while(1) {
        sleep(1);
        // printf("B, a = %d\n", a++);
        // printf("BBBBBBBBBBBB\n");
        // printf("%s ", args);
        console_put_str(str);
        // task_exit(current_task);
        // pause();
    }
}

int main()
{
    // int a;
    // test(NULL);
    printf("0x%lx\n", test);
    printf("sizeof(long int) = %ld\n", sizeof(long int));
    // console_init();
    // task_init();
    init();
    print_task_info(current_task);
    int a = 1;
    // task_start("test", 31, test, a);
    task_start("tast1", 31, test1, "argB ");
    for(int i = 0; i < 1000000; i++) {
        task_start("abc", 31, test1, "a ");
    }
    struct task_struct* ptask = tid2task(1);
    print_task_info(ptask);
    ptask = tid2task(2);
    print_task_info(ptask);
    // interrupt_enable();
    while(1) {
        sleep(1);
        console_put_str("maiN ");
        // pause();
    }
    return 0;
}
