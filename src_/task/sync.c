#include "sync.h"
#include "stdint.h"
#include "list.h"
#include "analog_interrupt.h"
#include "debug.h"
#include "assert.h"
#include "task.h"
#include <stdio.h>


/*初始化信号量*/
void sema_init(struct semaphore* psema, uint8_t value)
{
    psema->value = value;   //为信号量赋初值
    list_init(&psema->waiters);   //初始化信号量的等待队列
}

/*初始化锁plock*/
void lock_init(struct lock* plock)
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);   //信号量初值为1
}

/*信号量down操作*/
void sema_down(struct semaphore* psema)
{
    //关中断来保证原子操作
    interrupt_disable();
    while(psema->value == 0) {   //若value为0，表示已经被别人持有
        assert(!elem_find(&psema->waiters, &current_task->general_tag));
        //当前线程不应该已在信号量的waiters队列中
        if(elem_find(&psema->waiters, &current_task->general_tag)) {
            PANIC("sema_down: thread block has been in waiters_list\n");
        }
        //若信号量的值等于0，则当前线程把自己加入该锁的等待队列，然后阻塞自己
        list_append(&psema->waiters, &current_task->general_tag);
        task_block(TASK_BLOCKED);   //阻塞线程，直到被唤醒
    }
    //若value为1或被唤醒，会执行下面的代码，也就是获得了锁
    psema->value--;
    assert(psema->value == 0);
    //恢复之前的中断状态
    interrupt_enable();
}

/*信号量up操作*/
void sema_up(struct semaphore* psema)
{
    //关中断，保证原子操作
    interrupt_disable();
    assert(psema->value == 0);
    if(!list_empty(&psema->waiters)) {
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        task_unblock(thread_blocked);
    }
    psema->value++;
    assert(psema->value == 1);
    interrupt_enable();
}

/*获取锁plock*/
void lock_acquire(struct lock* plock)
{
    //排除曾经自己已经持有锁但还未将其释放的情况
    // printf("%p %p\n", plock->holder, current_task);
    if(plock->holder != current_task) {
        sema_down(&plock->semaphore);   //对信号量P操作，原子操作
        // printf("semaphore = %d\n", plock->semaphore);
        plock->holder = current_task;
        assert(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {
        plock->holder_repeat_nr++;
    }
}

/*释放锁plock*/
void lock_release(struct lock* plock)
{
    assert(plock->holder == current_task);
    if(plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    assert(plock->holder_repeat_nr == 1);

    plock->holder = NULL;   //吧锁的持有者置空放在V操作之前
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);   //信号量V操作，也是原子操作
}
