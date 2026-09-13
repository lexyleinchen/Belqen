#include <stdint.h>

#include "core/log.h"
#include "core/work.h"
#include "boot/multiboot.h"
#include "framebuffer/framebuffer.h"
#include "drivers/pci/pci.h"
#include "drivers/usb/usb.h"
#include "drivers/ide/ide.h"
#include "drivers/ps2/ps2.h"
#include "drivers/ethernet/ethernet.h"
#include "drivers/ethernet/e1000/e1000.h"
#include "network/arp/arp.h"
#include "network/ipv4/ipv4.h"
#include "network/ipv6/ipv6.h"
#include "network/icmp/icmp.h"
#include "network/udp/udp.h"
#include "network/dhcp/dhcp.h"
#include "storage/storage.h"
#include "../os/os.h"

void kernel_main(uint32_t multiboot_address) {
    log_init();
    kernel_log("PrintOS Kernel Starting...");
    multiboot_init(multiboot_address);
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
    storage_init();
    ps2_init();
    os_init();
    kernel_log("kernel started.");

    while (1) {
        ps2_poll();
        usb_poll();
        work_update();
        os_draw();
        e1000_poll();
    }
}