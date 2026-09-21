#include "scheduler.h"
#include "process.h"

Scheduler g_scheduler;

uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

void irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

void scheduler_enqueue_sleep(Thread* thread) {
    if (!thread) {
        return;
    }

    thread->wait_next = 0;
    thread->wait_previous = 0;

    if (!g_scheduler.sleep_head) {
        g_scheduler.sleep_head = thread;
        g_scheduler.sleep_tail = thread;
        return;
    }

    g_scheduler.sleep_tail->wait_next = thread;
    thread->wait_previous = g_scheduler.sleep_tail;
    g_scheduler.sleep_tail = thread;
}

static void scheduler_dequeue_sleep(Thread* thread) {
    if (!thread) {
        return;
    }

    if (g_scheduler.sleep_head == thread) {
        g_scheduler.sleep_head = thread->wait_next;
    }

    if (g_scheduler.sleep_tail == thread) {
        g_scheduler.sleep_tail = thread->wait_previous;
    }

    if (thread->wait_previous) {
        thread->wait_previous->wait_next = thread->wait_next;
    }

    if (thread->wait_next) {
        thread->wait_next->wait_previous = thread->wait_previous;
    }

    thread->wait_next = 0;
    thread->wait_previous = 0;
}

void scheduler_init(void) {
    g_scheduler.ready_head = 0;
    g_scheduler.ready_tail = 0;
    g_scheduler.sleep_head = 0;
    g_scheduler.sleep_tail = 0;
    g_scheduler.current = 0;
    g_scheduler.tick = 0;
    g_scheduler.quantum_ms = 10;
    g_scheduler.quantum_ticks = 1;
}

void scheduler_enqueue(Thread* thread) {
    if (!thread) {
        return;
    }

    thread->next = 0;
    thread->previous = 0;

    if (!g_scheduler.ready_head) {
        g_scheduler.ready_head = thread;
        g_scheduler.ready_tail = thread;
        return;
    }

    g_scheduler.ready_tail->next = thread;
    thread->previous = g_scheduler.ready_tail;
    g_scheduler.ready_tail = thread;
}

Thread* scheduler_dequeue(void) {
    Thread* thread = g_scheduler.ready_head;

    if (!thread) {
        return 0;
    }

    g_scheduler.ready_head = thread->next;

    if (!g_scheduler.ready_head) {
        g_scheduler.ready_tail = 0;
    }
    else {
        g_scheduler.ready_head->previous = 0;
    }

    thread->next = 0;
    thread->previous = 0;
    return thread;
}

Thread* scheduler_pick_next(void) {
    Thread* next = scheduler_dequeue();

    if (!next) {
        return 0;
    }

    next->state = THREAD_STATE_RUNNING;
    return next;
}

void scheduler_switch_to(Thread* next) {
    Thread* current = g_current_thread;

    if (!next) {
        return;
    }

    if (current == next) {
        return;
    }

    g_current_thread = next;
    g_current_process = next->process;
    g_scheduler.current = next;
    next->state = THREAD_STATE_RUNNING;

    if (current) {
        task_switch(&current->context, &next->context);
    }
}

void scheduler_wakeup_sleeping(void) {
    Thread* thread = g_scheduler.sleep_head;

    while (thread) {
        Thread* next = thread->wait_next;

        if (thread->state == THREAD_STATE_SLEEPING && thread->wake_tick <= g_scheduler.tick) {
            scheduler_dequeue_sleep(thread);
            thread->state = THREAD_STATE_READY;
            scheduler_enqueue(thread);
        }

        thread = next;
    }
}

void scheduler_tick(void) {
    g_scheduler.tick++;
    scheduler_wakeup_sleeping();
    Thread* current = g_current_thread;

    if (!current) {
        return;
    }

    if (current->state == THREAD_STATE_RUNNING) {
        current->state = THREAD_STATE_READY;
        scheduler_enqueue(current);
    }

    Thread* next = scheduler_pick_next();

    if (next) {
        scheduler_switch_to(next);
    }
}

void scheduler_yield(void) {
    thread_yield();
}