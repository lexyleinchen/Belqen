#include "thread.h"
#include "process.h"
#include "scheduler.h"

static Thread threads[THREAD_MAX_COUNT];
static Thread boot_thread;
static uint32_t next_tid = 1;
Thread* g_current_thread;

static Thread* thread_find_free_slot(void) {
    for (uint32_t i = 0; i < THREAD_MAX_COUNT; i++) {
        if (threads[i].state == THREAD_STATE_ZOMBIE && &threads[i] != g_current_thread) {
            threads[i].tid = 0;
            threads[i].pid = 0;
            threads[i].process = 0;
            threads[i].state = THREAD_STATE_DEAD;
        }

        if (threads[i].pid == 0 && threads[i].tid == 0) {
            return &threads[i];
        }
    }

    return 0;
}

static void thread_bootstrap(void) {
    Thread* thread = g_current_thread;

    if (!thread) {
        while (1) {
            __asm__ volatile ("sti; hlt");
        }
    }

    void (*entry)(void*) = (void (*)(void*))thread->entry;
    entry(thread->arg);
    thread_exit();

    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void thread_init_context(Thread* thread, void* entry, void* arg) {
    uint64_t stack_top;

    if (!thread) {
        return;
    }

    thread->entry = entry;
    thread->arg = arg;
    stack_top = ((uint64_t)thread->kernel_stack + THREAD_KERNEL_STACK_SIZE) & ~0xFULL;
    stack_top -= 8;
    *(uint64_t*)stack_top = 0;
    stack_top -= 8;
    *(uint64_t*)stack_top = (uint64_t)thread_bootstrap;
    stack_top -= 8;
    *(uint64_t*)stack_top = 0x202;
    thread->context.rax = 0;
    thread->context.rbx = 0;
    thread->context.rcx = 0;
    thread->context.rdx = 0;
    thread->context.rsi = 0;
    thread->context.rdi = (uint64_t)arg;
    thread->context.rbp = 0;
    thread->context.r8 = 0;
    thread->context.r9 = 0;
    thread->context.r10 = 0;
    thread->context.r11 = 0;
    thread->context.r12 = 0;
    thread->context.r13 = 0;
    thread->context.r14 = 0;
    thread->context.r15 = 0;
    thread->context.rsp = stack_top;
}

void thread_init(void) {
    for (uint32_t i = 0; i < THREAD_MAX_COUNT; i++) {
        threads[i].pid = 0;
        threads[i].tid = 0;
        threads[i].process = 0;
        threads[i].name[0] = 0;
        threads[i].state = THREAD_STATE_DEAD;
        threads[i].priority = 0;
        threads[i].flags = 0;
        threads[i].wake_tick = 0;
        threads[i].entry = 0;
        threads[i].arg = 0;
        threads[i].next = 0;
        threads[i].previous = 0;
        threads[i].wait_next = 0;
        threads[i].wait_previous = 0;
        
        for (uint32_t j = 0; j < THREAD_KERNEL_STACK_SIZE; j++) {
            threads[i].kernel_stack[j] = 0;
        }

        for (uint32_t j = 0; j < THREAD_USER_STACK_SIZE; j++) {
            threads[i].user_stack[j] = 0;
        }
    }

    next_tid = 1;
    scheduler_init();
    boot_thread.tid = 0;
    boot_thread.pid = 0;
    boot_thread.process = 0;
    boot_thread.name[0] = 'b';
    boot_thread.name[1] = 'o';
    boot_thread.name[2] = 'o';
    boot_thread.name[3] = 't';
    boot_thread.name[4] = 0;
    boot_thread.state = THREAD_STATE_RUNNING;
    g_current_thread = &boot_thread;
    g_scheduler.current = &boot_thread;
}

Thread* thread_create(struct Process* process, const char* name, void* entry, void* arg, uint32_t priority) {
    Thread* thread = thread_find_free_slot();

    if (!thread || !name || !process) {
        return 0;
    }

    if (process->thread_count >= 8) {
        return 0;
    }

    thread->tid = next_tid++;
    thread->pid = process->pid;
    thread->process = process;
    thread->state = THREAD_STATE_NEW;
    thread->priority = priority;
    thread->flags = 0;
    thread->wake_tick = 0;
    thread->quantum_ticks = 0;
    thread->entry = entry;
    thread->arg = arg;
    thread->next = 0;
    thread->previous = 0;
    thread->wait_next = 0;
    thread->wait_previous = 0;

    for (uint32_t i = 0; i < 31; i++) {
        thread->name[i] = name ? name[i] : 0;

        if (name && name[i] == 0) {
            break;
        }
    }

    thread->name[31] = 0;
    thread_init_context(thread, entry, arg);
    thread_set_state(thread, THREAD_STATE_READY);
    uint64_t flags = irq_save();
    scheduler_enqueue(thread);
    irq_restore(flags);
    process->threads[process->thread_count++] = thread;
    return thread;
}

Thread* thread_find_by_tid(uint32_t tid) {
    for (uint32_t i =0; i < THREAD_MAX_COUNT; i++) {
        if (threads[i].tid == tid) {
            return &threads[i];
        }
    }

    return 0;
}

void thread_set_state(Thread* thread, ThreadState state) {
    if (!thread) {
        return;
    }

    thread->state = state;
}

void thread_yield(void) {
    Thread* current = g_current_thread;

    if (!current) {
        return;
    }

    uint64_t flags = irq_save();
    Thread* next = scheduler_pick_next();

    if (next) {
        current->state = THREAD_STATE_READY;
        scheduler_enqueue(current);
        scheduler_switch_to(next);
    }

    irq_restore(flags);
}

void thread_exit(void) {
    Thread* current = g_current_thread;

    if (!current) {
        return;
    }

    current->state = THREAD_STATE_ZOMBIE;
    current->wake_tick = 0;
    Process* process = current->process;

    if (process) {
        for (uint32_t i = 0; i < process->thread_count; i++) {
            if (process->threads[i] == current) {
                process->threads[i] = process->threads[process->thread_count--];
                process->threads[process->thread_count] = 0;
                break;
            }
        }

        if (process->thread_count == 0) {
            process_exit(process, 0);
        }
    }

    Thread* next = scheduler_pick_next();

    if (next) {
        scheduler_switch_to(next);
    }

    while (1) {
        __asm__ volatile ("sti; hlt");
    }
}

void thread_sleep(uint64_t ms) {
    Thread* current = g_current_thread;

    if (!current) {
        return;
    }

    uint64_t flags = irq_save();
    Thread* next = scheduler_pick_next();

    if (next) {
        current->state = THREAD_STATE_SLEEPING;
        current->wake_tick = g_scheduler.tick + ms;
        scheduler_enqueue_sleep(current);
        scheduler_switch_to(next);
    }

    irq_restore(flags);
}