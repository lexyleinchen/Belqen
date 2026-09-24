#include <stdint.h>

#include "core/log.h"
#include "core/task.h"
#include "core/ipc.h"
#include "core/process.h"
#include "core/thread.h"
#include "core/scheduler.h"
#include "boot/multiboot.h"
#include "interrupts/interrupts.h"
#include "interrupts/apic.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "memory/address_space.h"
#include "framebuffer/framebuffer.h"
#include "drivers/pci/pci.h"
#include "drivers/usb/usb.h"
#include "drivers/ide/ide.h"
#include "drivers/ps2/ps2.h"
#include "drivers/ethernet/ethernet.h"
#include "drivers/ethernet/e1000/e1000.h"
#include "drivers/serial/serial.h"
#include "network/arp/arp.h"
#include "network/ipv4/ipv4.h"
#include "network/ipv6/ipv6.h"
#include "network/icmp/icmp.h"
#include "network/udp/udp.h"
#include "network/dhcp/dhcp.h"
#include "storage/storage.h"
#include "inputs/keyboard.h"
#include "inputs/mouse.h"
#include "../os/os.h"

static Process* ring3_test_process;

static void kernel_loop_thread(void* arg) {
    while (1) {
        keyboard_process();
        usb_poll();
        os_draw();
        e1000_poll();

        if (ring3_test_process && ring3_test_process->state == PROCESS_STATE_ZOMBIE) {
            ring3_test_process = 0;
            kernel_log("Ring 3 test exited.");
            log_disable_console();
            log_dump_serial();
        }

        thread_yield();
    }
}

static void ring3_test(void) {
    static const uint8_t user_code[] = {
        0xB8, 0x00, 0x00, 0x00, 0x00,
        0xCD, 0x80,
        0xB8, 0x02, 0x00, 0x00, 0x00,
        0x31, 0xFF,
        0xCD, 0x80
    };

    const uint64_t user_entry = USER_SPACE_START;
    const uint64_t code_size = VMM_PAGE_SIZE;
    Process* process = process_create_user("ring3_test");

    if (!process) {
        kernel_panic("ring 3 process creation failed");
    }

    AddressSpace* address_space = (AddressSpace*)process->address_space;

    if (!address_space_add_region(address_space, user_entry, code_size, 0)) {
        kernel_panic("ring 3 code region creation failed");
    }

    uint64_t physical = pmm_allocate_page();

    if (!physical) {
        kernel_panic("ring 3 code page allocation failed");
    }

    if (!address_space_map(address_space, user_entry, physical, 0)) {
        kernel_panic("ring 3 code page mapping failed");
    }

    uint64_t irq_flags = irq_save();
    uint64_t old_directory = vmm_get_current_directory();
    vmm_switch_directory(address_space->directory);

    for (uint64_t i = 0; i < sizeof(user_code); i++) {
        ((uint8_t*)(uintptr_t)user_entry)[i] = user_code[i];
    }

    vmm_switch_directory(old_directory);
    irq_restore(irq_flags);
    Thread* thread = thread_create_user(process, "ring3_test", (void*)(uintptr_t)user_entry, 1);

    if (!thread) {
        kernel_panic("ring 3 thread creation failed");
    }

    process_set_state(process, PROCESS_STATE_READY);
    ring3_test_process = process;
    kernel_log("Ring 3 test queued.");
}

void kernel_main(uint32_t multiboot_address) {
    log_init();
    serial_init();
    kernel_log("Belqen Kernel Starting...");
    multiboot_init(multiboot_address);
    log_init_console();
    pmm_init(multiboot_get_address());
    vmm_init();
    vmm_test();
    heap_init();
    heap_test();
    apic_init();
    interrupts_init();
    task_init();
    ipc_init();
    address_space_test();
    apic_timer_init(100);
    pci_init();
    ethernet_init();
    arp_init();
    ipv4_init();
    ipv6_init();
    icmp_init();
    udp_init();
    dhcp_init();
    dhcp_start();
    ide_init();
    ide_enable_interrupts();
    storage_init();
    ps2_init();
    os_init();
    kernel_log("kernel started.");
    Process* kernel_process = process_create("kernel");
    thread_create(kernel_process, "kernel_loop", (void*)kernel_loop_thread, 0, 1);
    ring3_test();

    while (1) {
        __asm__ volatile ("hlt");
    }
}