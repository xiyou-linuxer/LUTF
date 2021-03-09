#include "list.h"
#include <stdio.h>
#include <stdbool.h>

/*初始化双向链表*/
void list_init(struct list* list)
{
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

/*把链表元素elem插入在元素before之前*/
void list_insert_before(struct list_elem* before, struct list_elem* elem)
{
    // enum intr_status old_status = intr_disable();
    
    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;

    // intr_set_status(old_status);
}

/*添加元素到链表队首，类似栈push操作*/
void list_push(struct list* plist, struct list_elem* elem)
{
    list_insert_before(plist->head.next, elem);   //在队头插入elem
}

/*追加元素到链表对尾，类似队列的先进先出操作*/
void list_append(struct list* plist, struct list_elem* elem)
{
    list_insert_before(&plist->tail, elem);   //在队尾的前面插入
}

/*使元素pelem脱离链表*/
void list_remove(struct list_elem* pelem)
{
    // enum intr_status old_status = intr_disable();

    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;

    // intr_set_status(old_status);
}

/*将链表第一个元素弹出并返回，类似栈的pop操作*/
struct list_elem* list_pop(struct list* plist)
{
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

/*从链表中查找元素obj_elem，成功返回true，失败返回false*/
bool elem_find(struct list* plist, struct list_elem* obj_elem)
{
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail) {
        if(elem == obj_elem) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}

/*把列表plist中的每个元素elem和arg传给回调函数func，arg给func用来判断elem是否符合条件*/
struct list_elem* list_traversal(struct list* plist, function func, int arg)
{
    struct list_elem* elem = plist->head.next;
    //如果队列为空
    if(list_empty(plist)) {
        return NULL;
    }

    while(elem != &plist->tail) {
        if(func(elem, arg)) {   //func返回true，则认为该元素在回调函数中符合条件，命中，停止继续遍历
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}

/*返回链表长度*/
uint32_t list_len(struct list* plist)
{
    struct list_elem* elem = plist->head.next;
    uint32_t length = 0;
    while(elem != &plist->tail) {
        length++;
        elem = elem->next;
    }
    return length;
}

/*判断链表是否为空，空时返回true，否则返回false*/
bool list_empty(struct list* plist)
{
    return (plist->head.next == &plist->tail ? true : false);
}
