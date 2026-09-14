#include <stdint.h>

#include "core/log.h"
#include "core/work.h"
#include "boot/multiboot.h"
#include "interrupts/interrupts.h"
#include "interrupts/apic.h"
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

void kernel_main(uint32_t multiboot_address) {
    log_init();
    serial_init();
    kernel_log("PrintOS Kernel Starting...");
    multiboot_init(multiboot_address);
    log_init_console();
    apic_init();
    interrupts_init();
    apic_timer_init(1000);
    work_init();
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
    log_disable_console();
    log_dump_serial(); // DEBUG

    while (1) {
        keyboard_process();
        usb_poll();
        work_update();
        os_draw();
        e1000_poll();
    }
}