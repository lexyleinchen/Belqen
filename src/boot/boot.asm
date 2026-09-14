bits 32

section .multiboot

align 8

header_start:

    ; Multiboot header
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd -(0xE85250D6 + 0 + (header_end - header_start))

    ; Framebuffer request
    dw 5
    dw 0
    dd 20
    dd 0
    dd 0
    dd 32

    ; Padding so the next tag is aligned to 8 bytes
    dd 0

    ; End tag
    dw 0
    dw 0
    dd 8

header_end:



section .text

global _start
extern kernel_main

_start:
    mov esi, ebx
    mov esp, stack_top
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov eax, page_table_pml4
    mov cr3, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    lgdt [gdt_descriptor]
    jmp 0x08:long_mode_start

bits 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov edi, esi

    call kernel_main

.hang:
    hlt
    jmp .hang

section .rodata

align 4096

page_table_pml4:
    dq page_table_pdpt + 0x03
    times 511 dq 0

align 4096

page_table_pdpt:
    dq page_table_pd0 + 0x03
    dq page_table_pd1 + 0x03
    dq page_table_pd2 + 0x03
    dq page_table_pd3 + 0x03
    times 508 dq 0

align 4096

page_table_pd0:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x83
        %assign i i + 1
    %endrep

align 4096

page_table_pd1:
    %assign i 0
    %rep 512
        dq (0x40000000 + i * 0x200000) + 0x83
        %assign i i + 1
    %endrep

align 4096

page_table_pd2:
    %assign i 0
    %rep 512
        dq (0x80000000 + i * 0x200000) + 0x83
        %assign i i + 1
    %endrep

align 4096

page_table_pd3:
    %assign i 0
    %rep 512
        dq (0xC0000000 + i * 0x200000) + 0x83
        %assign i i + 1
    %endrep

section .data

align 8

gdt64:
    dq 0
    dq 0x00209A0000000000
    dq 0x0000920000000000

gdt64_end:

gdt_descriptor:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .bss

align 16

global stack_bottom
global stack_top

stack_bottom:
    resb 65536

stack_top:
