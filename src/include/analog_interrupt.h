#ifndef __ANALOG_INTERRUPT_H
#define __ANALOG_INTERRUPT_H

#include "stdint.h"

/**
 * interrupt_init - 中断启动
 * **/
void interrupt_init();

/**
 * interrupt_enable - 中断使能
 * **/
void interrupt_enable();

/**
 * interrupt_disable - shield interrupt
 * **/
void interrupt_disable();

#endif
