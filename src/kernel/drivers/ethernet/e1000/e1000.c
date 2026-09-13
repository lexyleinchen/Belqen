#include "e1000.h"
#include "../ethernet.h"
#include "../../pci/pci.h"
#include "../../../core/log.h"

#define E1000_VENDOR_ID 0x8086
#define PCI_BAR0 0x10
#define E1000_REG_CONTROL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_RAL 0x5400
#define E1000_REG_RAH 0x5404
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH 0x2810
#define E1000_REG_RDT 0x2818
#define E1000_REG_RCTL 0x0100
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH 0x3810
#define E1000_REG_TDT 0x3818
#define E1000_REG_TCTL 0x0400
#define E1000_REG_TIPG 0x0410
#define E1000_RX_DESC_COUNT 32
#define E1000_RX_STATUS_DD (1u << 0)
#define E1000_RX_STATUS_EOP (1u << 1)
#define E1000_TX_DESC_COUNT 32
#define E1000_TX_CMD_EOP (1u << 0)
#define E1000_TX_CMD_IFCS (1u << 1)
#define E1000_TX_CMD_RS (1u << 3)
#define E1000_TX_STATUS_DD (1u << 0)
#define E1000_PACKET_SIZE 2048
#define E1000_CONTROL_RST (1u << 26)
#define E1000_CONTROL_SLU (1u << 6)
#define PCI_BAR_IO_SPACE 0x1
#define PCI_BAR_MEM_MASK 0xFFFFFFF0

struct e1000_rx_desc {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t address;
    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status;
    uint8_t checksum_start;
    uint16_t special;
} __attribute__((packed));

static volatile uint32_t* e1000_regs = 0;
static uint32_t e1000_tx_index = 0;
static uint32_t e1000_rx_index = 0;
static uint8_t e1000_mac[6];

static struct e1000_rx_desc
    rx_descriptors[E1000_RX_DESC_COUNT]
    __attribute__((aligned(16)));

static struct e1000_tx_desc
    tx_descriptors[E1000_TX_DESC_COUNT]
    __attribute__((aligned(16)));

static uint8_t
    rx_buffers[E1000_RX_DESC_COUNT][E1000_PACKET_SIZE]
    __attribute__((aligned(16)));

static uint8_t
    tx_buffers[E1000_TX_DESC_COUNT][E1000_PACKET_SIZE]
    __attribute__((aligned(16)));

static uint32_t e1000_read(uint16_t offset) {
    return e1000_regs[offset / 4];
}

static void e1000_write(uint16_t offset, uint32_t value) {
    e1000_regs[offset / 4] = value;
}

static void e1000_reset(void) {
    kernel_log("E1000 resetting...");
    uint32_t control = e1000_read(E1000_REG_CONTROL);
    e1000_write(E1000_REG_CONTROL, control | E1000_CONTROL_RST);

    for (volatile uint32_t i = 0; i < 100000; i++) {
        (void)e1000_read(E1000_REG_STATUS);
    }

    kernel_log("E1000 reset complete.");
}

static void e1000_read_mac(uint8_t mac[6]) {
    uint32_t ral = e1000_read(E1000_REG_RAL);
    uint32_t rah = e1000_read(E1000_REG_RAH);
    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);
}

static void e1000_init_rx(void) {
    kernel_log("E1000 initializing rx...");

    for (uint32_t i =0; i < E1000_RX_DESC_COUNT; i++) {
        rx_descriptors[i].address = (uint64_t)(uintptr_t)&rx_buffers[i][0];
        rx_descriptors[i].length = 0;
        rx_descriptors[i].checksum = 0;
        rx_descriptors[i].status = 0;
        rx_descriptors[i].errors = 0;
        rx_descriptors[i].special = 0;
    }

    uint32_t rx_address = (uint32_t)(uintptr_t)&rx_descriptors[0];
    e1000_write(E1000_REG_RDBAL, rx_address);
    e1000_write(E1000_REG_RDBAH, 0);
    e1000_write(E1000_REG_RDLEN, sizeof(rx_descriptors));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, E1000_RX_DESC_COUNT - 1);
    uint32_t rctl = (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 15) | (1u << 26);
    e1000_write(E1000_REG_RCTL, rctl);
    kernel_log("E1000 rx initialized.");
}

static void e1000_init_tx(void) {
    kernel_log("E1000 initializing tx...");

    for (uint32_t i =0; i < E1000_TX_DESC_COUNT; i++) {
        tx_descriptors[i].address = (uint64_t)(uintptr_t)&tx_buffers[i][0];
        tx_descriptors[i].length = 0;
        tx_descriptors[i].checksum_offset = 0;
        tx_descriptors[i].command = 0;
        tx_descriptors[i].status = E1000_TX_STATUS_DD;
        tx_descriptors[i].checksum_start = 0;
        tx_descriptors[i].special = 0;
    }

    uint32_t tx_address = (uint32_t)(uintptr_t)&tx_descriptors[0];
    e1000_write(E1000_REG_TDBAL, tx_address);
    e1000_write(E1000_REG_TDBAH, 0);
    e1000_write(E1000_REG_TDLEN, sizeof(tx_descriptors));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);
    uint32_t tctl = (1u << 1) | (1u << 3) | (0x40u << 12);
    e1000_write(E1000_REG_TCTL, tctl);
    e1000_write(E1000_REG_TIPG, 0x0060200A);
    kernel_log("E1000 tx initialized.");
}

int e1000_send(const uint8_t* data, uint16_t length) {
    if (length > E1000_PACKET_SIZE) {
        kernel_log("E1000 packet too large");
        return 0;
    }

    if (length > ETHERNET_MAX_FRAME_SIZE) {
        kernel_log("E1000 ethernet frame too large");
        return 0;
    }

    uint32_t index = e1000_tx_index;

    if (!(tx_descriptors[index].status & E1000_TX_STATUS_DD)) {
        kernel_log("E1000 tx descriptor busy");
        return 0;
    }

    for (uint16_t i = 0; i < length; i++) {
        tx_buffers[index][i] = data[i];
    }

    if (length < ETHERNET_MIN_FRAME_SIZE) {
        for (uint16_t i = length; i < ETHERNET_MIN_FRAME_SIZE; i++) {
            tx_buffers[index][i] = 0;
        }

        length = ETHERNET_MIN_FRAME_SIZE;
    }

    tx_descriptors[index].length = length;
    tx_descriptors[index].checksum_offset = 0;
    tx_descriptors[index].checksum_start = 0;
    tx_descriptors[index].special = 0;
    tx_descriptors[index].command = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
    tx_descriptors[index].status = 0;
    e1000_tx_index = (index + 1) % E1000_TX_DESC_COUNT;
    e1000_write(E1000_REG_TDT, e1000_tx_index);
    return 1;
}

static int e1000_receive(uint8_t* buffer, uint16_t buffer_size) {
    uint32_t index = e1000_rx_index;
    struct e1000_rx_desc* desc = &rx_descriptors[index];

    if (!(desc->status & E1000_RX_STATUS_DD)) {
        return 0;
    }

    kernel_log("E1000 rx descriptor %u ready length %u status %u", index, desc->length, desc->status);

    if (!(desc->status & E1000_RX_STATUS_EOP)) {
        kernel_log("E1000 rx packet is split.");
        return -1;
    }

    uint16_t length = desc->length;

    if (length > buffer_size) {
        kernel_log("E1000 rx packet too large.");
        return -1;
    }

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = rx_buffers[index][i];
    }

    desc->status = 0;
    e1000_write(E1000_REG_RDT, index);
    e1000_rx_index = (index + 1) % E1000_RX_DESC_COUNT;
    return length;
}

void e1000_poll(void) {
    uint8_t packet[E1000_PACKET_SIZE];
    int length = e1000_receive(packet, sizeof(packet));
    
    if (length <= 0) {
        return;
    }

    if (length < ETHERNET_HEADER_SIZE) {
        kernel_log("E1000 recived invalid ethernet frame.");
        return;
    }

    ethernet_receive(packet, (uint16_t)length);
}

void e1000_get_mac(uint8_t mac[6]) {
    for (uint32_t i = 0; i < 6; i++) {
        mac[i] = e1000_mac[i];
    }
}

static void e1000_init(void) {
    kernel_log("E1000 initializing dma...");
    e1000_init_rx();
    e1000_init_tx();
    kernel_log("E1000 dma initialization complete.");
}

void e1000_controller_found(uint8_t bus, uint8_t slot, uint8_t function) {
    uint16_t vendor_id = pci_config_read16(bus, slot, function, 0x00);
    uint16_t device_id = pci_config_read16(bus, slot, function, 0x02);

    if (vendor_id != E1000_VENDOR_ID) {
        return;
    }

    kernel_log("Intel ethernet controller found");
    kernel_log("E1000 vendor %u", vendor_id);
    kernel_log("E1000 device %u", device_id);
    pci_enable_bus_mastering(bus, slot, function);
    uint32_t bar0 = pci_config_read32(bus, slot, function, PCI_BAR0);
    kernel_log("E1000 bar0 %u", bar0);

    if (bar0 & PCI_BAR_IO_SPACE) {
        kernel_log("E1000 bar0 is io space.");
        kernel_log("E1000 io mode is not supported yet.");
        return;
    }

    uint32_t mmio_base = bar0 & PCI_BAR_MEM_MASK;
    kernel_log("E1000 mmio base %u", mmio_base);
    e1000_regs = (volatile uint32_t*)mmio_base;
    uint32_t status = e1000_read(E1000_REG_STATUS);
    kernel_log("E1000 status %u", status);
    e1000_reset();
    e1000_write(E1000_REG_CONTROL, e1000_read(E1000_REG_CONTROL) | E1000_CONTROL_SLU);
    status = e1000_read(E1000_REG_STATUS);
    kernel_log("E1000 status after reset %u", status);
    e1000_read_mac(e1000_mac);
    kernel_log("E1000 mac %x:%x:%x:%x:%x:%x", e1000_mac[0], e1000_mac[1], e1000_mac[2], e1000_mac[3], e1000_mac[4], e1000_mac[5]);
    e1000_init();
    kernel_log("E1000 initialization complete.");
}