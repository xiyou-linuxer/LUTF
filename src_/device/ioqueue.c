#include "ioqueue.h"
#include "analog_interrupt.h"
#include "stdint.h"
#include "debug.h"
#include "assert.h"
#include "console.h"
#include <stdio.h>

/*初始化io队列ioq*/
void ioqueue_init(struct ioqueue* ioq)
{
    lock_init(&ioq->lock);   //初始化io队列的锁
    ioq->producer = ioq->consumer = NULL;   //生产者和消费者置空
    ioq->head = ioq->tail = 0;   //队列的收尾指针指向缓冲区数组第0个位置
}

/*返回pos在缓冲区中的下一个位置值*/
static int32_t next_pos(int32_t pos)
{
    return (pos + 1) % bufsize;
}

/*判断队列是否已满*/
bool ioq_full(struct ioqueue* ioq)
{
    // assert(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

/*判断队列是否已空*/
bool ioq_empty(struct ioqueue* ioq)
{
    // assert(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

/*使当前生产者或消费者在此缓冲区上等待*/
static void ioq_wait(struct task_struct** waiter)
{
    assert(*waiter == NULL && waiter != NULL);
    *waiter = current_task;
    task_block(TASK_BLOCKED);
}

/*唤醒waiter*/
static void wakeup(struct task_struct** waiter)
{
    assert(*waiter != NULL);
    task_unblock(*waiter);
    *waiter = NULL;
}

/*消费者从ioq队列中获取一个字符*/
char ioq_getchar(struct ioqueue* ioq)
{
    // assert(intr_get_status() == INTR_OFF);

    /*若缓冲区为空，把消费者ioq_consumer记为当前线程自己,
      目的是将来生产者往缓冲区里装商品后，生产者知道唤醒哪个消费者，
      也就是唤醒当前线程自己*/
    while(ioq_empty(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char byte = ioq->buf[ioq->tail];   //从缓冲区中取出
    ioq->tail = next_pos(ioq->tail);   //吧读游标移到下一个位置

    if(ioq->producer != NULL) {
        wakeup(&ioq->producer);   //唤醒生产者
    }

    return byte;
}

/*生产者往ioq队列中写入一个字符byte*/
void ioq_putchar(struct ioqueue* ioq, char byte)
{
    // assert(intr_get_status() == INTR_OFF);

    /*若缓冲区已经满了，吧生产者ioq->producer记为自己，
      为的是当缓冲区里的东西被消费者取完后让消费者知道唤醒哪个线程,
      也就是唤醒当前线程自己*/
    while(ioq_full(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    ioq->buf[ioq->head] = byte;   //白字节放入缓冲区
    ioq->head = next_pos(ioq->head);   //吧游标移到下一位置

    if(ioq->consumer != NULL) {
        wakeup(&ioq->consumer);
    }
}

/*返回环形缓冲区中的数据长度*/
uint32_t ioq_length(struct ioqueue* ioq)
{
    uint32_t len = 0;
    if(ioq->head >= ioq->tail) {
        len = ioq->head - ioq->tail;
    } else {
        len = bufsize - (ioq->tail - ioq->head);
    }
    return len;
}
