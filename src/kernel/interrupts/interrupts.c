#include "interrupts.h"
#include "apic.h"
#include "../inputs/keyboard.h"
#include "../memory/vmm.h"
#include "../core/scheduler.h"
#include "../core/log.h"

#define IDT_ENTRY_COUNT 256
#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xA0
#define PIC_SLAVE_DATA 0xA1
#define PIC_END_OF_INTERRUPT 0x20
#define PIC_ICW1_INIT 0x10
#define PIC_ICW1_ICW4 0x01
#define PIC_ICW4_8086 0x01
#define PIC_READ_ISR 0x0B
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40
#define PIT_FREQUENCY 1193182
#define PIT_TARGET_FREQUENCY 100
#define DECLARE_ISR(number) extern void isr##number(void);

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) IdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) Idtr;

typedef void (*InterruptStub)(void);

extern void interrupts_load_idt(const Idtr* idtr);
extern void isr_default(void);
extern uint8_t stack_bottom[];
extern uint8_t stack_top[];

DECLARE_ISR(0);
DECLARE_ISR(1);
DECLARE_ISR(2);
DECLARE_ISR(3);
DECLARE_ISR(4);
DECLARE_ISR(5);
DECLARE_ISR(6);
DECLARE_ISR(7);
DECLARE_ISR(8);
DECLARE_ISR(9);
DECLARE_ISR(10);
DECLARE_ISR(11);
DECLARE_ISR(12);
DECLARE_ISR(13);
DECLARE_ISR(14);
DECLARE_ISR(15);
DECLARE_ISR(16);
DECLARE_ISR(17);
DECLARE_ISR(18);
DECLARE_ISR(19);
DECLARE_ISR(20);
DECLARE_ISR(21);
DECLARE_ISR(22);
DECLARE_ISR(23);
DECLARE_ISR(24);
DECLARE_ISR(25);
DECLARE_ISR(26);
DECLARE_ISR(27);
DECLARE_ISR(28);
DECLARE_ISR(29);
DECLARE_ISR(30);
DECLARE_ISR(31);
DECLARE_ISR(32);
DECLARE_ISR(33);
DECLARE_ISR(34);
DECLARE_ISR(35);
DECLARE_ISR(36);
DECLARE_ISR(37);
DECLARE_ISR(38);
DECLARE_ISR(39);
DECLARE_ISR(40);
DECLARE_ISR(41);
DECLARE_ISR(42);
DECLARE_ISR(43);
DECLARE_ISR(44);
DECLARE_ISR(45);
DECLARE_ISR(46);
DECLARE_ISR(47);
DECLARE_ISR(48);
DECLARE_ISR(49);
DECLARE_ISR(50);
DECLARE_ISR(51);
DECLARE_ISR(52);
DECLARE_ISR(53);
DECLARE_ISR(54);
DECLARE_ISR(55);
DECLARE_ISR(56);
DECLARE_ISR(57);
DECLARE_ISR(58);
DECLARE_ISR(59);
DECLARE_ISR(60);
DECLARE_ISR(61);
DECLARE_ISR(62);
DECLARE_ISR(63);
DECLARE_ISR(64);

static IdtEntry idt[IDT_ENTRY_COUNT];
static volatile uint64_t timer_ticks = 0;
static InterruptHandler irq_handlers[16];
static InterruptHandler high_vector_handlers[64];

static InterruptStub interrupt_stubs[64] = {
    isr0, isr1, isr2, isr3,
    isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11,
    isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19,
    isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27,
    isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35,
    isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43,
    isr44, isr45, isr46, isr47,
    isr48, isr49, isr50, isr51,
    isr52, isr53, isr54, isr55,
    isr56, isr57, isr58, isr59,
    isr60, isr61, isr62, isr63,
};

static const char* exception_names[32] = {
    "divide error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid tss",
    "segment not present",
    "stack segment faul",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating-point execption",
    "alignment check",
    "machine check",
    "simd floating-point execption",
    "virtualization execption",
    "control protection exception",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection exception",
    "vmm communication exception",
    "security exception",
    "reserved"
};

static void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void interrupts_disable(void) {
    __asm__ volatile ("cli");
}

static void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

static uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static uint8_t pic_read_isr(uint16_t command_port) {
    outb(command_port, PIC_READ_ISR);
    return inb(command_port);
}

static void idt_set_gate(uint8_t vector, InterruptStub handler) {
    uint64_t address = (uint64_t)handler;

    idt[vector].offset_low = address & 0xFFFF;
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].type_attributes = 0x8E;
    idt[vector].offset_middle = (address >> 16) & 0xFFFF;
    idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

static void pic_remap(void) {
    uint8_t master_mask;
    uint8_t slave_mask;
    master_mask = 0xFF;
    slave_mask = 0xFF;
    outb(PIC_MASTER_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC_SLAVE_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC_MASTER_DATA, 32);
    io_wait();
    outb(PIC_SLAVE_DATA, 40);
    io_wait();
    outb(PIC_MASTER_DATA, 4);
    io_wait();
    outb(PIC_SLAVE_DATA, 2);
    io_wait();
    outb(PIC_MASTER_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC_SLAVE_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA, slave_mask);
}

static void pic_send_end_of_interrupt(uint64_t vector) {
    if (vector >= 40) {
        outb(PIC_SLAVE_COMMAND, PIC_END_OF_INTERRUPT);
    }

    if (vector >= 32 && vector < 48) {
        outb(PIC_MASTER_COMMAND, PIC_END_OF_INTERRUPT);
    }
}

static int pic_is_spurious_irq(uint8_t irq) {
    if (irq == 7) {
        return (pic_read_isr(PIC_MASTER_COMMAND) & 0x80) == 0;
    }

    if (irq == 15) {
        return (pic_read_isr(PIC_SLAVE_COMMAND) & 0x80) == 0;
    }

    return 0;
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_sleep(uint64_t ms) {
    uint64_t start = timer_get_ticks();
    uint64_t duration = (ms + 9) / 10;

    while (timer_get_ticks() - start < duration) {
        __asm__ volatile ("hlt");
    }
}

static void pit_init(void) {
    uint16_t divisor = PIT_FREQUENCY / PIT_TARGET_FREQUENCY;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, divisor >> 8);
    kernel_log("Pit timer initialized at 100hz.");
}

static void timer_interrupt_handler(void) {
    timer_ticks++;
    pic_send_end_of_interrupt(32);
    scheduler_tick();
}

static void apic_timer_interrupt_handler(void) {
    timer_ticks++;
    apic_send_end_of_interrupt();
    scheduler_tick();
}

static void exception_print_registers(InterruptFrame* frame) {
    kernel_log("rax %x | rbx %x | rcx %x | rdx %x | rsi %x | rdi %x | rbp %x | rsp %x", (uint32_t)(frame->rax & 0xFFFFFFFF), (uint32_t)(frame->rbx & 0xFFFFFFFF), (uint32_t)(frame->rcx & 0xFFFFFFFF), (uint32_t)(frame->rdx & 0xFFFFFFFF), (uint32_t)(frame->rsi & 0xFFFFFFFF), (uint32_t)(frame->rdi & 0xFFFFFFFF), (uint32_t)(frame->rbp & 0xFFFFFFFF), (uint32_t)(frame->rsp & 0xFFFFFFFF));
    kernel_log("r8 %x | r9 %x | r10 %x | r11 %x | r12 %x | r13 %x | r14 %x | r15 %x", (uint32_t)(frame->r8 & 0xFFFFFFFF), (uint32_t)(frame->r9 & 0xFFFFFFFF), (uint32_t)(frame->r10 & 0xFFFFFFFF), (uint32_t)(frame->r11 & 0xFFFFFFFF), (uint32_t)(frame->r12 & 0xFFFFFFFF), (uint32_t)(frame->r13 & 0xFFFFFFFF), (uint32_t)(frame->r14 & 0xFFFFFFFF), (uint32_t)(frame->r15 & 0xFFFFFFFF));
    kernel_log("rflags %x | cs %x | ss %x", (uint32_t)(frame->rflags & 0xFFFFFFFF), (uint32_t)(frame->cs & 0xFFFFFFFF), (uint32_t)(frame->ss & 0xFFFFFFFF));
}

static void exception_divide_by_zero(InterruptFrame* frame) {
    kernel_log("Divide by zero at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("divide by zero error");
}

static void exception_debug(InterruptFrame* frame) {
    kernel_log("Debug exception at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
}

static void exception_nmi(InterruptFrame* frame) {
    kernel_log("Non-maskable interrupt");
    exception_print_registers(frame);
    kernel_panic("nmi received");
}

static void exception_breakpoint(InterruptFrame* frame) {
    kernel_log("Breakpoint at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
}

static void exception_overflow(InterruptFrame* frame) {
    kernel_log("Overflow exception at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("overflow error");
}

static void exception_bound_range(InterruptFrame* frame) {
    kernel_log("Bound range exceeded at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("bound range exceeded");
}

static void exception_invalid_opcode(InterruptFrame* frame) {
    kernel_log("Invalid opcode at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("invalid opcode");
}

static void exception_device_not_available(InterruptFrame* frame) {
    kernel_log("Device not available (fpu?)");
    kernel_panic("device not available");
}

static void exception_double_fault(InterruptFrame* frame) {
    kernel_log("DOUBLE FAULT - CRITICAL!");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("double fault");
}

static void exception_coprocessor_overrun(InterruptFrame* frame) {
    kernel_log("Coprocessor segment overrun.");
    kernel_panic("coprocessor overrun");
}

static void exception_invalid_tss(InterruptFrame* frame) {
    kernel_log("Invalid tss.");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("invalid tss");
}

static void exception_segment_not_present(InterruptFrame* frame) {
    kernel_log("Segment not present.");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("segment not present");
}

static void exception_stack_fault(InterruptFrame* frame) {
    kernel_log("Stack segment fault.");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("stack segment fault");
}

static void exception_general_protection_fault(InterruptFrame* frame) {
    kernel_log("General protection fault at rip %x", (uint32_t)(frame->rip & 0xFFFFFFFF));
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    uint64_t selector = (frame->error_code >> 3) & 0x1FFF;
    uint64_t ext = (frame->error_code >> 16) & 0x1;
    kernel_log("selector %x | external %x", (uint32_t)selector, (uint32_t)ext);
    exception_print_registers(frame);
    kernel_panic("general protection fault");
}

static void exception_page_fault(InterruptFrame* frame) {
    uint64_t fault_address = read_cr2();
    uint64_t error_code = frame->error_code;
    const char* type = "unknown";

    if (!(error_code & 0x1)) {
        type = "not-present";
    }
    else if (error_code & 0x2) {
        type = "write";
    }
    else if (error_code & 0x4) {
        type = "user-mode";
    }

    if (vmm_handle_page_fault(fault_address, error_code)) {
        return;
    }

    kernel_log("Page fault at address %x (type %s)", (uint32_t)(fault_address & 0xFFFFFFFF), type);
    kernel_log("Error code %x | rip %x", (uint32_t)(error_code & 0xFFFFFFFF), (uint32_t)(frame->rip & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("unrecoverable page fault");
}

static void exception_x87_fpu(InterruptFrame* frame) {
    kernel_log("x87 fpu exception.");
    kernel_panic("x87 fpu exception");
}

static void exception_alignment_check(InterruptFrame* frame) {
    kernel_log("Alignment check exception.");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("alignment check");
}

static void exception_machine_check(InterruptFrame* frame) {
    kernel_log("MACHINE CHECK - HARDWARE ERROR!");
    exception_print_registers(frame);
    kernel_panic("machine check exception");
}

static void exception_simd_fpu(InterruptFrame* frame) {
    kernel_log("Simd fpu exception.");
    kernel_panic("simd fpu exception");
}

static void exception_virtualization(InterruptFrame* frame) {
    kernel_log("Virtualization exception.");
    kernel_panic("virtualization exception");
}

static void exception_control_protection(InterruptFrame* frame) {
    kernel_log("Control protection exception.");
    kernel_log("Error code %x", (uint32_t)(frame->error_code & 0xFFFFFFFF));
    exception_print_registers(frame);
    kernel_panic("control protection exception");
}

void interrupts_init(void) {
    Idtr idtr;
    interrupts_disable();

    for (uint32_t i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attributes = 0;
        idt[i].offset_middle = 0;
        idt[i].offset_high = 0;
        idt[i].reserved = 0;
    }

    for (uint32_t i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt_set_gate((uint8_t)i, isr_default);
    }

    for (uint32_t i = 0; i < 64; i++) {
        idt_set_gate((uint8_t)i, interrupt_stubs[i]);
    }

    idt_set_gate(64, isr64);
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt[0];
    interrupts_load_idt(&idtr);
    pic_remap();

    if (!apic_is_initialized()) {
        pit_init();
        interrupt_register_irq(0, timer_interrupt_handler);
        outb(PIC_MASTER_DATA, 0xFE);
    }
    else if (!apic_has_ioapic()) {
        outb(PIC_MASTER_DATA, 0xFF);
    }
    else {
        outb(PIC_MASTER_DATA, 0xFF);
    }

    outb(PIC_SLAVE_DATA, 0xFF);
    kernel_log("Interrupt descriptor table initialized.");
    kernel_log("Pic remapped to vectors 32-47.");
    interrupts_enable();
}

void interrupt_dispatch(InterruptFrame* frame) {
    if (!frame) {
        kernel_panic("interrupt frame was null.");
    }

    if (frame->vector < 32) {
        const char* name = exception_names[frame->vector];
        kernel_log("Cpu exception %u %s", (uint32_t)frame->vector, name);

        switch (frame->vector) {
            case 0: 
                exception_divide_by_zero(frame);
                break;
            case 1: 
                exception_debug(frame);
                break;
            case 2: 
                exception_nmi(frame);
                break;
            case 3: 
                exception_breakpoint(frame);
                break;
            case 4: 
                exception_overflow(frame);
                break;
            case 5: 
                exception_bound_range(frame);
                break;
            case 6: 
                exception_invalid_opcode(frame);
                break;
            case 7: 
                exception_device_not_available(frame);
                break;
            case 8: 
                exception_double_fault(frame);
                break;
            case 9: 
                exception_coprocessor_overrun(frame);
                break;
            case 10: 
                exception_invalid_tss(frame);
                break;
            case 11: 
                exception_segment_not_present(frame);
                break;
            case 12: 
                exception_stack_fault(frame);
                break;
            case 13: 
                exception_general_protection_fault(frame);
                break;
            case 14: 
                exception_page_fault(frame);
                break;
            case 16: 
                exception_x87_fpu(frame);
                break;
            case 17: 
                exception_alignment_check(frame);
                break;
            case 18: 
                exception_machine_check(frame);
                break;
            case 19: 
                exception_simd_fpu(frame);
                break;
            case 20: 
                exception_virtualization(frame);
                break;
            case 21: 
                exception_control_protection(frame);
                break;
            default:
                kernel_log("Unhandled cpu exception %u", (uint32_t)frame->vector);
                exception_print_registers(frame);
                kernel_panic(name);
                break;
        }
    }

    if (frame->vector >= 32 && frame->vector < 48) {
        uint8_t irq = (uint8_t)(frame->vector - 32);

        if (!apic_is_initialized() && pic_is_spurious_irq(irq)) {
            if (irq == 15) {
                outb(PIC_MASTER_COMMAND, PIC_END_OF_INTERRUPT);
            }

            return;
        }

        if (irq_handlers[irq]) {
            irq_handlers[irq]();
        }
        else {
            kernel_log("Unhandled irq %u", (uint32_t)irq);
        }

        if (apic_is_initialized()) {
            apic_send_end_of_interrupt();
        }
        else if (irq != 0) {
            pic_send_end_of_interrupt(frame->vector);
        }
    }

    if (frame->vector == 64) {
        apic_timer_interrupt_handler();
        return;
    }

    if (frame->vector >= 48) {
        uint8_t handler_index = frame->vector - 48;

        if (handler_index < 64 && high_vector_handlers[handler_index]) {
            high_vector_handlers[handler_index]();
        }
        else {
            kernel_log("Unexpected interrupt vector %u", (uint32_t)frame->vector);
            kernel_log("Unexpected interrupt vector.");
        }

        if (apic_is_initialized()) {
            apic_send_end_of_interrupt();
        }
    }
}

void interrupt_register_irq(uint8_t irq, InterruptHandler handler) {
    if (irq >= 16) {
        return;
    }

    irq_handlers[irq] = handler;
}

void interrupt_register_vector(uint8_t vector, InterruptHandler handler) {
    if (vector < 48 || vector >= 112) {
        return;
    }

    high_vector_handlers[vector - 48] = handler;
}

void interrupt_unmask_irq(uint8_t irq) {
    if (apic_is_initialized() && apic_has_ioapic()) {
        return;
    }

    uint16_t port;
    uint8_t mask;
    uint8_t irq_to_unmask = irq;

    if (irq < 8) {
        port = PIC_MASTER_DATA;
    }
    else if (irq < 16) {
        port = PIC_SLAVE_DATA;
        irq_to_unmask = irq - 8;
        mask = inb(PIC_MASTER_DATA);
        mask &= (uint8_t)~(1 << 2);
        outb(PIC_MASTER_DATA, mask);
    }
    else {
        return;
    }

    mask = inb(port);
    mask &= (uint8_t)~(1 << irq_to_unmask);
    outb(port, mask);
}

void kernel_stack_trace(void) {
    uint64_t* frame_pointer;
    uint64_t stack_bottom_address = (uint64_t)stack_bottom;
    uint64_t stack_top_address = (uint64_t)stack_top;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(frame_pointer));
    kernel_log("Kernel stack trace");

    for (uint32_t depth = 0; depth < 16; depth++) {
        uint64_t frame_address = (uint64_t)frame_pointer;

        if (frame_address < stack_bottom_address || frame_address + 16 > stack_top_address) {
            break;
        }

        uint64_t return_address = frame_pointer[1];

        if (return_address == 0) {
            break;
        }

        kernel_log("#%u rip low %x high %x", depth, (uint32_t)(return_address & 0xFFFFFFFF), (uint32_t)(return_address >> 32));
        uint64_t* previous_frame = (uint64_t*)frame_pointer[0];

        if ((uint64_t)previous_frame <= frame_address) {
            break;
        }

        frame_pointer = previous_frame;
    }
}

void kernel_panic(const char* reason) {
    interrupts_disable();
    kernel_log("KERNEL PANIC %s", reason);
    kernel_stack_trace();
    
    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void panic_test_level_three(void) {
    volatile uint64_t* invalid_address = (uint64_t*)0xFFFFFFFFFFFFF000;
    *invalid_address = 1;
}

static void panic_test_level_two(void) {
    panic_test_level_three();
}

void panic_test_level_one(void) {
    panic_test_level_two();
}