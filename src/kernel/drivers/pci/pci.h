#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PciDevice {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} PciDevice;

void pci_init(void);

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);

uint32_t pci_get_bar_size(uint8_t bus, uint8_t slot, uint8_t function, uint8_t bar_offset);

uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t function);

#ifdef __cplusplus
}
#endif

#endif // PCI_H