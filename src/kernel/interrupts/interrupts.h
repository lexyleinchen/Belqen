#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*InterruptHandler)(void);

typedef struct InterruptFrame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} InterruptFrame;

void interrupts_init(void);

void interrupt_dispatch(InterruptFrame* frame);

void interrupt_register_irq(uint8_t irq, InterruptHandler handler);

void interrupt_register_vector(uint8_t vector, InterruptHandler handler);

void interrupt_unmask_irq(uint8_t irq);

uint64_t timer_get_ticks(void);

void timer_sleep(uint64_t ms);

void kernel_panic(const char* reason);

void kernel_stack_trace(void);

void panic_test_level_one(void);

#ifdef __cplusplus
}
#endif

#endif // INTERRUPTS_H