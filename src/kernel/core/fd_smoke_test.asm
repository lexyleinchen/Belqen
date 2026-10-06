bits 64
default rel

global ring3_basic_start
global ring3_basic_end
global fd_smoke_start
global fd_smoke_end

%define USER_BUFFER 0x0000000300001000

section .text

ring3_basic_start:
    mov eax, 3
    mov edi, 1
    lea rsi, [rel ring3_basic_message]
    mov edx, 11
    int 0x80

    mov eax, 2
    xor edi, edi
    int 0x80

ring3_basic_message: db 'RING3 PASS', 10

ring3_basic_end:

fd_smoke_start:
    mov eax, 6
    lea rdi, [rel missing_path]
    int 0x80
    cmp rax, -1
    jne .failed

    mov eax, 6
    lea rdi, [rel test_path]
    int 0x80
    cmp rax, -1
    je .failed
    mov r12, rax

    mov eax, 7
    mov rdi, r12
    mov rsi, USER_BUFFER
    mov edx, 6
    int 0x80
    cmp rax, 6
    jne .failed

    lea rsi, [rel original_bytes]
    mov rdi, USER_BUFFER
    mov ecx, 6
    repe cmpsb
    jne .failed
    
    mov eax, 8
    mov rdi, r12
    xor esi, esi
    xor edx, edx
    int 0x80
    cmp rax, -1
    je .failed

    mov eax, 3
    mov rdi, r12
    lea rsi, [rel replacement_bytes]
    mov edx, 6
    int 0x80
    cmp rax, 6
    jne .failed

    mov eax, 8
    mov rdi, r12
    xor esi, esi
    xor edx, edx
    int 0x80
    cmp rax, -1
    je .failed

    mov eax, 7
    mov rdi, r12
    mov rsi, USER_BUFFER
    mov edx, 6
    int 0x80
    cmp rax, 6
    jne .failed

    lea rsi, [rel replacement_bytes]
    mov rdi, USER_BUFFER
    mov ecx, 6
    repe cmpsb
    jne .failed

    mov eax, 9
    mov rdi, r12
    int 0x80
    cmp rax, -1
    je .failed

    mov eax, 9
    mov rdi, r12
    int 0x80
    cmp rax, -1
    jne .failed

    mov eax, 6
    lea rdi, [rel test_path]
    int 0x80
    cmp rax, -1
    je .failed
    mov r12, rax

    mov eax, 3
    mov edi, 1
    lea rsi, [rel pass_message]
    mov edx, 8
    int 0x80

    mov eax, 2
    xor edi, edi
    int 0x80
    jmp .halt

.failed:
    mov eax, 3
    mov edi, 1
    lea rsi, [rel fail_message]
    mov edx, 8
    int 0x80

    mov eax, 2
    mov edi, 1
    int 0x80

.halt:
    hlt
    jmp .halt

missing_path: db '/fd-smoke-missing', 0
test_path: db '/fd-smoke-test.txt', 0
original_bytes: db 'before'
replacement_bytes: db 'after'
pass_message: db 'FD PASS', 10
fail_message: db 'FD FAIL', 10

fd_smoke_end: