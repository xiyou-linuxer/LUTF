#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "stdint.h"
#include "list.h"
#include "task.h"

/*信号量结构*/
struct semaphore
{
    uint8_t value;
    struct list waiters;
};

/*锁结构*/
struct lock
{
    struct task_struct* holder;   //锁的持有者
    struct semaphore semaphore;   //用二元信号量实现锁
    uint32_t holder_repeat_nr;    //锁的持有者重复申请锁的次数
};

void sema_init(struct semaphore* psema, uint8_t value);
void lock_init(struct lock* plock);
/*信号量down操作*/
void sema_down(struct semaphore* psema);
/*信号量up操作*/
void sema_up(struct semaphore* psema);
/*获取锁plock*/
void lock_acquire(struct lock* plock);
/*释放锁plock*/
void lock_release(struct lock* plock);

#endif
