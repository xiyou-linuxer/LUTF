#ifndef __CONCOLE_H
#define __CONCOLE_H

/**
 * console_init - 初始化终端
 * **/
void console_init();

/**
 * console_acquire - 获取终端
 * **/
void console_acquire();

/**
 * console_release - 释放终端
 * **/
void console_release();

/**
 * console_put_str - 输出字符串
 * **/
void console_put_str(char* str);

/**
 * console_put_char - 输出字符
 * **/
void console_put_char(char ch);

#endif
