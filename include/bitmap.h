#ifndef __BITMAP_H
#define __BITMAP_H

#include "stdint.h"
#include <stdbool.h>

#define BITMAP_MASK 1

struct bitmap
{
    uint32_t btmp_bytes_len;   //在遍历位图时，整体上以字节为单位，细节上以位为单位
    uint8_t* bits;
};

/**
 * bitmap_init - 位图btmp初始化
 * @btmp: 要初始化的位图
 * **/
void bitmap_init(struct bitmap* btmp);

/**
 * bitmap_scan_test - 判断bit_idx位是否为1，若为1，则返回true，否则返回false
 * @btmp: 操作的位图
 * @bit_idx: 第bit_idx个位
 * **/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);

/**
 * bitmap_scan - 在位图中申请连续cnt个位，成功，则返回其起始下标，失败，返回-1
 * @btmp: 操作的位图
 * @cnt: 连续申请cnt个位
 * **/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);

/**
 * bitmap_set - 将位图btmp的bit_idx位置为value
 * @btmp: 操作的位图
 * @bit_idx: 位的索引
 * @value: 要设置的值
 * **/
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);

#endif
