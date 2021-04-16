#include "debug.h"
#include "analog_interrupt.h"
#include <stdio.h>

/*打印文件名，行号，函数名，条件并使程序悬停*/
void panic_spin(char* filename, \
                int line, \
                const char* func, \
                const char* condition)
{
    interrupt_disable();   //因为有时候会单独调用panic_spin,所以在此处关中断
    fprintf(stderr,"\n\n\n!!!!!! error !!!!!!\n");
    fprintf(stderr,"filename:%s\n", filename);
    fprintf(stderr,"line:0x%x\n", line);
    fprintf(stderr,"function:%s\n", (char*)func);
    fprintf(stderr,"condition:%s\n", (char*)condition);
    while(1);
}
