#include "../include/task.h"
#include "console.h"
#include "init.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void test1(void* args)
{
    co_enable_hook_sys();
    char* str = (char*)args;
    int timeout = atoi(str);
    char arr[30];
    sprintf(arr, "test%d : %d.\n",timeout,timeout);
    while(1) {
        sleep(timeout);
        console_put_str(arr);
    }
}

const static int thread_number = 10; 

int main()
{
    init();
    task_start("tast1", 31, test1, (void*)"1");
    char* pool[thread_number];
    for(int i = 1; i <= thread_number; i++) {
        char* arr = (char*)malloc(30);
        pool[i-1] = arr;
        sprintf(arr, "%d", i);
        task_start(arr, 31, test1, (void*)pool[i-1]);
    }
    while(1) {
        //sleep(1); // TODO 现在我们还不能在主协程调用
        console_put_str("");
    }

    for (size_t i = 0; i < thread_number; i++){
        free(pool[i]);
    }
    
    return 0;
}
