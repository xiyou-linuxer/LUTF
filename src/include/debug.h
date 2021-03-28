#ifndef ___DEBUG_H
#define ___DEBUG_H

void panic_spin(char* filename, int line, const char* func, const char* condition);

/******* _VA_ARGS_ ******
 * _VA_ARGS是预处理器所支持的专用标识符
 * 代表所有与省略号相对应的参数
 * "..."表示定义的宏其参数可变
 * ***********************/
#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define ASSERT(CONDITION) ((void)0)
#else
    #define ASSERT(CONDITION)\
        if(CONDITION) {} else { \
    /*符号#让编译器将宏的参数转化为字符串面量*/ \
        PANIC(#CONDITION); \
    }
#endif /*NDEBUG*/

#endif /*_KERNEL_DEBUG_H*/
