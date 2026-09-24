bits 64

section .text

extern interrupt_dispatch

global interrupts_load_idt

interrupts_load_idt:
    lidt [rdi]
    ret

global isr_default

isr_default:
    push 0
    push 255
    jmp interrupt_common

%macro ISR_NO_ERROR 1
global isr%1

isr%1:
    push 0
    push %1
    jmp interrupt_common
%endmacro

%macro ISR_ERROR 1
global isr%1

isr%1:
    push %1
    jmp interrupt_common
%endmacro

global isr128

isr128:
    push 0
    push 128
    jmp interrupt_common

interrupt_common:
    cld

    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    sub rsp, 8
    call interrupt_dispatch
    add rsp, 8

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR 8
ISR_NO_ERROR 9
ISR_ERROR 10
ISR_ERROR 11
ISR_ERROR 12
ISR_ERROR 13
ISR_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR 29
ISR_ERROR 30
ISR_NO_ERROR 31
ISR_NO_ERROR 32
ISR_NO_ERROR 33
ISR_NO_ERROR 34
ISR_NO_ERROR 35
ISR_NO_ERROR 36
ISR_NO_ERROR 37
ISR_NO_ERROR 38
ISR_NO_ERROR 39
ISR_NO_ERROR 40
ISR_NO_ERROR 41
ISR_NO_ERROR 42
ISR_NO_ERROR 43
ISR_NO_ERROR 44
ISR_NO_ERROR 45
ISR_NO_ERROR 46
ISR_NO_ERROR 47
ISR_NO_ERROR 48
ISR_NO_ERROR 49
ISR_NO_ERROR 50
ISR_NO_ERROR 51
ISR_NO_ERROR 52
ISR_NO_ERROR 53
ISR_NO_ERROR 54
ISR_NO_ERROR 55
ISR_NO_ERROR 56
ISR_NO_ERROR 57
ISR_NO_ERROR 58
ISR_NO_ERROR 59
ISR_NO_ERROR 60
ISR_NO_ERROR 61
ISR_NO_ERROR 62
ISR_NO_ERROR 63
ISR_NO_ERROR 64