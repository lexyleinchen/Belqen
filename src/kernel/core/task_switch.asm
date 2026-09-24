bits 64

section .text

global task_switch

task_switch:
    pushfq

    ; Save currnet context
    mov [rdi + 0x00], r15
    mov [rdi + 0x08], r14
    mov [rdi + 0x10], r13
    mov [rdi + 0x18], r12
    mov [rdi + 0x20], r11
    mov [rdi + 0x28], r10
    mov [rdi + 0x30], r9
    mov [rdi + 0x38], r8
    mov [rdi + 0x40], rbp
    mov [rdi + 0x48], rsi
    mov [rdi + 0x50], rdi
    mov [rdi + 0x58], rdx
    mov [rdi + 0x60], rcx
    mov [rdi + 0x68], rbx
    mov [rdi + 0x70], rax
    mov [rdi + 0x78], rsp

    mov r11, rsi

    ; Restore next context
    mov r15, [r11 + 0x00]
    mov r14, [r11 + 0x08]
    mov r13, [r11 + 0x10]
    mov r12, [r11 + 0x18]
    mov r10, [r11 + 0x28]
    mov r9, [r11 + 0x30]
    mov r8, [r11 + 0x38]
    mov rbp, [r11 + 0x40]
    mov rsi, [r11 + 0x48]
    mov rdi, [r11 + 0x50]
    mov rdx, [r11 + 0x58]
    mov rcx, [r11 + 0x60]
    mov rbx, [r11 + 0x68]
    mov rax, [r11 + 0x70]
    mov rsp, [r11 + 0x78]
    mov r11, [r11 + 0x20]

    popfq
    ret

global user_thread_bootstrap
extern user_thread_prepare

user_thread_bootstrap:
    call user_thread_prepare
    iretq