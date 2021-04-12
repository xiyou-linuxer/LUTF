#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "stdint.h"
#include <stdbool.h>

#define offset(struct_type,member) (uintptr_t)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
            (struct_type*)((char*)elem_ptr - offset(struct_type, struct_member_name))

/******* 定义链表节点成员结构 *******/
struct list_elem
{
    struct list_elem* prev;   //前驱节点
    struct list_elem* next;   //后继节点
};

/*链表结构，用来实现队列*/
struct list
{
    struct list_elem head;   //队首，是固定不变的，不是第一个元素，第一个元素为head.next
    struct list_elem tail;   //队尾，同样是固定不变的
};

/*自定义函数类型function，用于在list_trabersal中做回调函数*/
typedef bool (function)(struct list_elem*, int arg);

void list_init(struct list* list);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
bool list_empty(struct list* list);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);

#endif
