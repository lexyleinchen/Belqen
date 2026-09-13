#include "pci.h"
#include "../usb/usb.h"
#include "../ahci/ahci.h"
#include "../ide/ide.h"
#include "../ethernet/e1000/e1000.h"
#include "../../core/log.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC) | 0x80000000;
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC) | 0x80000000;
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, slot, function, offset);
    if (offset & 2) {
        return (uint16_t)(value >> 16);
    } else {
        return (uint16_t)value;
    }
}

uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, slot, function, offset);
    return (uint8_t)(value >> ((offset & 3) * 8));
}

static void pci_scan(void) {
    kernel_log("scanning pci devices...");
    int device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t class_code;
        uint16_t subclass;
        uint16_t prog_if;
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t function = 0; function < 8; function++) {
                vendor_id = pci_config_read16(bus, slot, function, 0x00);
                if (vendor_id == 0xFFFF) {
                    continue; // No device present
                }

                device_id = pci_config_read16(bus, slot, function, 0x02);
                class_code = pci_config_read8(bus, slot, function, 0x0B);
                subclass = pci_config_read8(bus, slot, function, 0x0A);
                prog_if = pci_config_read8(bus, slot, function, 0x09);

                (void)device_id;
                (void)class_code;
                (void)subclass;
                (void)prog_if;

                kernel_log("found pci device");
                kernel_log("vendor_id: %u", vendor_id);
                kernel_log("device_id: %u", device_id);
                kernel_log("class_code: %u", class_code);
                kernel_log("subclass: %u", subclass);
                kernel_log("prog_if: %u", prog_if);

                if (class_code == 0x0C && subclass == 0x03) {
                    usb_controller_found(bus, slot, function, prog_if);
                }

                if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
                    ahci_controller_found(bus, slot, function, prog_if);
                }

                if (class_code == 0x01 && subclass == 0x01) {
                    ide_controller_found(bus, slot, function, prog_if);
                }

                if (class_code == 0x02 && subclass == 0x00) {
                    e1000_controller_found(bus, slot, function);
                }

                device_count++;
            }
        }
    }
    kernel_log("finished scanning pci devices. found %u devices", device_count);
}

uint32_t pci_get_bar_size(uint8_t bus, uint8_t slot, uint8_t function, uint8_t bar_offset) {
    uint32_t original = pci_config_read32(bus, slot, function, bar_offset);
    pci_config_write32(bus, slot, function, bar_offset, 0xFFFFFFFF);
    uint32_t mask = pci_config_read32(bus, slot, function, bar_offset);
    pci_config_write32(bus, slot, function, bar_offset, original);
    mask &= 0xFFFFFFFC;

    if (mask == 0) {
        return 0;
    }

    return (~mask) + 1;
}

void pci_init(void) {
    kernel_log("initializing pci...");
    pci_scan();
    kernel_log("pci started.");
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t function) {
    uint16_t command = pci_config_read16(bus, slot, function, 0x04);
    command |= 0x0004;
    uint32_t value = pci_config_read32(bus, slot, function, 0x04);
    value &= 0xFFFF0000;
    value |= command;
    pci_config_write32(bus, slot, function, 0x04, value);
}