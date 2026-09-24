#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_MAX_COUNT 128
#define THREAD_KERNEL_STACK_SIZE 16384
#define THREAD_USER_STACK_SIZE 4096
#define THREAD_FLAG_USER 0x00000001
#define THREAD_USER_STACK_TOP 0x00007FFFFFFFE000ULL

typedef enum {
    THREAD_STATE_NEW = 0,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_SLEEPING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_ZOMBIE,
    THREAD_STATE_DEAD
} ThreadState;

typedef struct ThreadContext {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rsp;
} ThreadContext;

struct Process;

typedef struct Thread {
    uint32_t tid;
    uint32_t pid;
    struct Process* process;
    char name[32];
    ThreadState state;
    uint32_t priority;
    uint32_t flags;
    uint64_t wake_tick;
    uint32_t quantum_ticks;
    void* entry;
    void* arg;
    ThreadContext context;
    uint8_t kernel_stack[THREAD_KERNEL_STACK_SIZE];
    uint8_t user_stack[THREAD_USER_STACK_SIZE];
    struct Thread* next;
    struct Thread* previous;
    struct Thread* wait_next;
    struct Thread* wait_previous;
} Thread;

void thread_init(void);

Thread* thread_create(struct Process* process, const char* name, void* entry, void* arg, uint32_t priority);

Thread* thread_create_user(struct Process* process, const char* name, void* entry, uint32_t priority);

Thread* thread_find_by_tid(uint32_t tid);

void thread_set_state(Thread* thread, ThreadState state);

void thread_yield(void);

void thread_mark_exit(uint32_t exit_code);

void thread_exit(void);

void thread_sleep(uint64_t ms);

extern Thread* g_current_thread;

extern void task_switch(ThreadContext* current, ThreadContext* next);

#ifdef __cplusplus
}
#endif

#endif // THREAD_H