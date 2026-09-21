#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Scheduler {
    Thread* ready_head;
    Thread* ready_tail;
    Thread* sleep_head;
    Thread* sleep_tail;
    Thread* current;
    uint64_t tick;
    uint32_t quantum_ms;
    uint32_t quantum_ticks;
} Scheduler;

uint64_t irq_save(void);

void irq_restore(uint64_t flags);

void scheduler_init(void);

void scheduler_enqueue(Thread* thread);

void scheduler_enqueue_sleep(Thread* thread);

Thread* scheduler_dequeue(void);

Thread* scheduler_pick_next(void);

void scheduler_switch_to(Thread* next);

void scheduler_tick(void);

void scheduler_wakeup_sleeping(void);

void scheduler_yield(void);

extern Scheduler g_scheduler;

#ifdef __cplusplus
}
#endif

#endif // SCHEDULER_H