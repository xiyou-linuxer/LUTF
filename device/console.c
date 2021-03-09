#include "console.h"
#include "sync.h"
#include "task.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

static struct lock concole_lock;   //控制台锁

static char io_buffer[1024];
static int fionbio_attr = 1;

/**
 * console_init - 初始化终端
 * **/
void console_init()
{
    lock_init(&concole_lock);
    //设置输入输出为非阻塞
    ioctl(STDIN_FILENO, FIONBIO, &fionbio_attr);
    ioctl(STDOUT_FILENO, FIONBIO, &fionbio_attr);
}

/**
 * console_acquire - 获取终端
 * **/
void console_acquire()
{
    lock_acquire(&concole_lock);
}

/**
 * console_release - 释放终端
 * **/
void console_release()
{
    lock_release(&concole_lock);
}

/**
 * console_put_str - 输出字符串
 * **/
void console_put_str(char* str)
{
    console_acquire();
    //输出字符串
    write(STDOUT_FILENO, str, strlen(str));
    console_release();
}
