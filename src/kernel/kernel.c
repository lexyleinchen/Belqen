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
#include "storage/vfs.h"
#include "inputs/keyboard.h"
#include "inputs/mouse.h"
#include "../os/os.h"

static Process* ring3_test_process;
static int ring3_test_is_fd_smoke;
static int kernel_started = 0;

extern const uint8_t ring3_basic_start[];
extern const uint8_t ring3_basic_end[];
extern const uint8_t fd_smoke_start[];
extern const uint8_t fd_smoke_end[];

static void kernel_loop_thread(void* arg) {
    while (1) {
        if (kernel_started) {
            keyboard_process();
            usb_poll();
            os_draw();
            e1000_poll();
        }
        else {
            if (ring3_test_process && ring3_test_process->state == PROCESS_STATE_ZOMBIE) {

                if (ring3_test_is_fd_smoke) {
                    int descriptors_closed = 1;

                    for (uint32_t fd = 3; fd < PROCESS_MAX_FILE_DESCRIPTORS; fd++) {
                        if (ring3_test_process->file_descriptors[fd].used) {
                            descriptors_closed = 0;
                            break;
                        }
                    }

                    if (!vfs_delete_file("/fd-smoke-test.txt")) {
                        kernel_log("Deleting file descriptor fixture failed.");
                    }

                    kernel_log(descriptors_closed ? "File descriptor exit cleanup passed." : "File descriptor exit cleanup failed.");
                }

                ring3_test_process = 0;
                kernel_log("Ring 3 test exited.");
                kernel_log("kernel started.");
                log_disable_console();
                log_dump_serial();
                kernel_started = 1;
            }
        }

        thread_yield();
    }
}

static void fd_smoke_prepare_file(void) {
    const char* path = "/fd-smoke-test.txt";
    FilesystemFile file;

    if (vfs_open_path(path, &file)) {
        vfs_close(&file);

        if (!vfs_delete_file(path)) {
            kernel_panic("file descriptor smoke test could not remove old fixture");
        }
    }

    if (!vfs_create_file_path(path) || !vfs_open_path(path, &file)) {
        kernel_panic("file descriptor smoke test could not create fixture");
    }

    static const char contents[] = "before";
    uint32_t bytes_written = 0;

    if (!vfs_write(&file, contents, 6, &bytes_written) || bytes_written != 6) {
        vfs_close(&file);
        kernel_panic("file descriptor smoke test could not write fixture");
    }

    vfs_close(&file);
}

static void ring3_test(void) {
    int run_fd_smoke = storage_get_filesystem_count() > 0;
    const uint8_t* image_start = ring3_basic_start;
    const uint8_t* image_end = ring3_basic_end;

    if (run_fd_smoke) {
        fd_smoke_prepare_file();
        image_start = fd_smoke_start;
        image_end = fd_smoke_end;
    }
    else {
        kernel_log("No filesystem we will run basic ring 3 test.");
    }

    const uint64_t user_entry = USER_SPACE_START;
    const uint64_t user_buffer = USER_SPACE_START + VMM_PAGE_SIZE;
    const uint64_t code_bytes = (uint64_t)(image_end - image_start);

    if (code_bytes > VMM_PAGE_SIZE) {
        kernel_panic("ring 3 image is too large");
    }

    Process* process = process_create_user("ring3_test");

    if (!process) {
        kernel_panic("ring 3 process creation failed");
    }

    AddressSpace* address_space = (AddressSpace*)process->address_space;

    if (!address_space_add_region(address_space, user_entry, VMM_PAGE_SIZE, 0)) {
        kernel_panic("ring 3 code region creation failed");
    }

    uint64_t physical = pmm_allocate_page();

    if (!physical) {
        kernel_panic("ring 3 code page allocation failed");
    }

    if (!address_space_map(address_space, user_entry, physical, 0)) {
        kernel_panic("ring 3 code page mapping failed");
    }

    if (run_fd_smoke) {
        if (!address_space_add_region(address_space, user_buffer, VMM_PAGE_SIZE, VMM_WRITABLE)) {
            kernel_panic("file descriptor smoke test buffer region creation failed");
        }

        uint64_t buffer_physical = pmm_allocate_page();

        if (!buffer_physical || !address_space_map(address_space, user_buffer, buffer_physical, VMM_WRITABLE)) {
            kernel_panic("file descriptor smoke test buffer mapping failed");
        }
    }

    uint64_t irq_flags = irq_save();
    uint64_t old_directory = vmm_get_current_directory();
    vmm_switch_directory(address_space->directory);

    for (uint64_t i = 0; i < code_bytes; i++) {
        ((uint8_t*)(uintptr_t)user_entry)[i] = image_start[i];
    }

    vmm_switch_directory(old_directory);
    irq_restore(irq_flags);
    Thread* thread = thread_create_user(process, "ring3_test", (void*)(uintptr_t)user_entry, 1);

    if (!thread) {
        kernel_panic("ring 3 thread creation failed");
    }

    process_set_state(process, PROCESS_STATE_READY);
    ring3_test_process = process;
    ring3_test_is_fd_smoke = run_fd_smoke;
    kernel_log(run_fd_smoke ? "File descriptor ring 3 test queued." : "Basic ring 3 test queued.");
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
    ring3_test();
    Process* kernel_process = process_create("kernel");
    thread_create(kernel_process, "kernel_loop", (void*)kernel_loop_thread, 0, 1);

    while (1) {
        __asm__ volatile ("hlt");
    }
}