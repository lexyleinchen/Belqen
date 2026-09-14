#include "apic.h"
#include "../boot/multiboot.h"
#include "../core/log.h"

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_ENABLE 0x800
#define APIC_BASE_ADDRESS 0xFEE00000
#define APIC_REG_EOI 0x0B0
#define APIC_REG_ID 0x020
#define APIC_REG_SPURIOUS 0x0F0
#define APIC_REG_TPR 0x080
#define APIC_LVT_MASKED (1u << 16)
#define APIC_REG_LVT_TIMER 0x320
#define APIC_REG_TIMER_INITIAL 0x380
#define APIC_REG_TIMER_CURRENT 0x390
#define APIC_REG_TIMER_DIVIDE 0x3E0
#define APIC_TIMER_PERIODIC (1u << 17)
#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10
#define IOAPIC_REDIRECTION0 0x10
#define PIT_FREQUENCY 1193182
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) AcpiRsdp;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) AcpiSdtHeader;

typedef struct {
    AcpiSdtHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) AcpiMadt;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) AcpiIoApicEntry;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) AcpiInterruptOverride;

static volatile uint32_t* apic_base = 0;
static int apic_available = 0;
static uint32_t local_apic_id = 0;
static volatile uint32_t* ioapic_base = 0;
static uint32_t ioapic_gsi_base = 0;

static void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile ("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf));
}

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint64_t read_msr(uint32_t msr) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low; 
}

static void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static void imcr_switch_to_apic_mode(void) {
    outb(0x22, 0x70);
    outb(0x23, 0x01);
}

static uint32_t ioapic_read(uint32_t reg) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    return ioapic_base[IOAPIC_WINDOW / 4];
}

static void ioapic_write(uint32_t reg, uint32_t value) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    ioapic_base[IOAPIC_WINDOW / 4] = value;
}

static void ioapic_route_gsi(uint32_t gsi, uint8_t vector, int active_low, int level_triggered) {
    uint32_t reg = IOAPIC_REDIRECTION0 + (gsi * 2);
    uint32_t low = (uint32_t)vector;

    if (active_low) {
        low |= (1u << 13);
    }

    if (level_triggered) {
        low |= (1u << 15);
    }

    ioapic_write(reg, low);
    ioapic_write(reg + 1, local_apic_id << 24);
}

static void ioapic_verify_route(uint32_t gsi, uint8_t vector) {
    uint32_t reg = IOAPIC_REDIRECTION0 + (gsi * 2);
    uint32_t low = ioapic_read(reg);
    uint32_t high = ioapic_read(reg + 1);
    kernel_log("Ioapic gsi %u low %x high %x", gsi, low, high);

    if ((low & 0xFF) != vector) {
        kernel_log("Ioapic route mismatch for gsi %u", gsi);
    }
}

static uint32_t apic_find_override_for_irq(const AcpiMadt* madt, uint8_t irq, uint16_t* flags_out) {
    if (!madt) {
        return UINT32_MAX;
    }

    const uint8_t* current = (const uint8_t*)madt + sizeof(AcpiMadt);
    const uint8_t* end = (const uint8_t*)madt + madt->header.length;

    while (current < end) {
        uint8_t type = current[0];
        uint8_t length = current[1];

        if (type == 2 && length >= sizeof(AcpiInterruptOverride)) {
            const AcpiInterruptOverride* override = (const AcpiInterruptOverride*)current;

            if (override->irq_source == irq) {
                if (flags_out) {
                    *flags_out = override->flags;
                }

                kernel_log("Override found for irq %u | gsi %u | flags %x", irq, override->gsi, override->flags);
                return override->gsi;
            }
        }

        current += length;
    }

    return UINT32_MAX;
}

static void apic_route_irqs(const AcpiMadt* madt, uint8_t irq, uint8_t vector) {
    uint16_t flags = 0;
    uint32_t gsi = apic_find_override_for_irq(madt, irq, &flags);
    int active_low = 0;
    int level_triggered = 0;

    if (gsi == UINT32_MAX) {
        gsi = irq;
        active_low = 0;
        level_triggered = 0;
    }
    else {
        uint32_t polarity = flags & 0x3;
        uint32_t trigger = (flags >> 2) & 0x3;
        active_low = (polarity == 3) ? 1 : 0;
        level_triggered = (trigger == 3) ? 1 : 0;
    }

    ioapic_route_gsi(gsi, vector, active_low, level_triggered);
    ioapic_verify_route(gsi, vector);
}

static void apic_route_device_irqs(const AcpiMadt* madt) {
    apic_route_irqs(madt, 1, 33);
    apic_route_irqs(madt, 12, 44);
    apic_route_irqs(madt, 14, 46);
}

static void ioapic_init_from_madt(const AcpiMadt* madt) {
    if (!madt) {
        return;
    }

    const uint8_t* current = (const uint8_t*)madt + sizeof(AcpiMadt);
    const uint8_t* end = (const uint8_t*)madt + madt->header.length;

    while (current < end) {
        const AcpiIoApicEntry* entry = (const AcpiIoApicEntry*)current;

        if (entry->type == 1 && entry->length >= sizeof(AcpiIoApicEntry)) {
            ioapic_base = (volatile uint32_t*)(uintptr_t)entry->io_apic_address;
            ioapic_gsi_base = entry->global_system_interrupt_base;
            kernel_log("Ioapic found at %x gsi base %x", entry->io_apic_address, entry->global_system_interrupt_base);
            return;
        }

        current += entry->length;
    }
}

static int acpi_checksum(const uint8_t* data, uint32_t length) {
    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + data[i]);
    }

    return sum == 0;
}

static int acpi_signature_valid(const AcpiRsdp* rsdp) {
    static const char expected_signature[8] = "RSD PTR ";

    for (uint32_t i = 0; i < 8; i++) {
        if (rsdp->signature[i] != expected_signature[i]) {
            return 0;
        }
    }

    return 1;
}

static int acpi_table_valid(const AcpiSdtHeader* table) {
    if (!table) {
        return 0;
    }

    if (table->length < sizeof(AcpiSdtHeader)) {
        return 0;
    }

    return acpi_checksum((const uint8_t*)table, table->length);
}

static AcpiMadt* acpi_find_madt(const AcpiRsdp* rsdp) {
    if (!rsdp) {
        return 0;
    }

    if (!acpi_signature_valid(rsdp)) {
        kernel_log("Invalid acpi rsdp signature.");
        return 0;
    }

    kernel_log("Rsdp revision %u rsdt %x", (uint32_t)rsdp->revision, rsdp->rsdt_address);

    if (rsdp->revision != 0 && rsdp->revision < 2) {
        kernel_log("Unsupported acpi rsdp revision %u", (uint32_t)rsdp->revision);
        return 0;
    }

    kernel_log("Rsdp bytes 8-15 %x %x %x %x %x %x %x %x", ((const uint8_t*)rsdp)[8], ((const uint8_t*)rsdp)[9], ((const uint8_t*)rsdp)[10], ((const uint8_t*)rsdp)[11], ((const uint8_t*)rsdp)[12], ((const uint8_t*)rsdp)[13], ((const uint8_t*)rsdp)[14], ((const uint8_t*)rsdp)[15]);

    if (!acpi_checksum((const uint8_t*)rsdp, 20)) {
        kernel_log("Invalid acpi rsdp checksum.");
        return 0;
    }

    if (rsdp->revision >= 2) {
        if (rsdp->length < 36) {
            kernel_log("Invvalid acpi rsdp length %u", rsdp->length);
            return 0;
        }

        if (!acpi_checksum((const uint8_t*)rsdp, rsdp->length)) {
            kernel_log("Invalid extended acpi rsdp checksum.");
            return 0;
        }

        kernel_log("Xdst address low %x high %x", (uint32_t)(rsdp->xsdt_address & 0xFFFFFFFF), (uint32_t)(rsdp->xsdt_address >> 32));
    }

    AcpiSdtHeader* root;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        root = (AcpiSdtHeader*)(uintptr_t)rsdp->xsdt_address;

        if (!acpi_table_valid(root)) {
            kernel_log("Invalid acpi root table.");
            return 0;
        }

        uint32_t entry_count = (root->length - sizeof(AcpiSdtHeader)) / sizeof(uint64_t);
        uint64_t* entries = (uint64_t*)((uint8_t*)root + sizeof(AcpiSdtHeader));

        for (uint32_t i = 0; i < entry_count; i++) {
            AcpiSdtHeader* table = (AcpiSdtHeader*)(uintptr_t)entries[i];

            if (table->signature[0] == 'A' && table->signature[1] == 'P' && table->signature[2] == 'I' && table->signature[3] == 'C') {
                return (AcpiMadt*)table;
            }
        }
    }
    else {
        root = (AcpiSdtHeader*)(uintptr_t)rsdp->rsdt_address;

        if (!acpi_table_valid(root)) {
            kernel_log("Invalid acpi root table.");
            return 0;
        }

        uint32_t entry_count = (root->length - sizeof(AcpiSdtHeader)) / sizeof(uint32_t);
        uint32_t* entries = (uint32_t*)((uint8_t*)root + sizeof(AcpiSdtHeader));

        for (uint32_t i = 0; i < entry_count; i++) {
            AcpiSdtHeader* table = (AcpiSdtHeader*)(uintptr_t)entries[i];

            if (table->signature[0] == 'A' && table->signature[1] == 'P' && table->signature[2] == 'I' && table->signature[3] == 'C') {
                return (AcpiMadt*)table;
            }
        }
    }

    return 0;
}

int apic_is_initialized(void) {
    return apic_available;
}

int apic_is_available(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & (1u << 9)) != 0;
}

int apic_has_ioapic(void) {
    return ioapic_base != 0;
}

void apic_route_irq(uint8_t irq, uint8_t vector) {
    if (!ioapic_base) {
        return;
    }

    const AcpiRsdp* rsdp = (const AcpiRsdp*)multiboot_get_rsdp();
    AcpiMadt* madt = acpi_find_madt(rsdp);

    if (madt) {
        apic_route_irqs(madt, irq, vector);
    }
}

void apic_init(void) {
    if (!apic_is_available()) {
        kernel_log("Local apic unavailable.");
        return;
    }

    const AcpiRsdp* rsdp = (const AcpiRsdp*)multiboot_get_rsdp();
    AcpiMadt* madt = acpi_find_madt(rsdp);

    if (!madt) {
        kernel_log("Acpi madt not found.");
        return;
    }

    kernel_log("Madt local apic address %x", madt->local_apic_address);
    kernel_log("Madt flags %x", madt->flags);
    uint64_t apic_msr = read_msr(IA32_APIC_BASE_MSR);
    apic_msr |= IA32_APIC_ENABLE;
    write_msr(IA32_APIC_BASE_MSR, apic_msr);
    apic_base = (volatile uint32_t*)(uintptr_t)madt->local_apic_address;
    apic_base[APIC_REG_SPURIOUS / 4] = 0xFF | (1u << 8);
    local_apic_id = (apic_base[APIC_REG_ID / 4] >> 24) & 0xFF;
    kernel_log("Local apic id %x", local_apic_id);
    ioapic_base = (volatile uint32_t*)(uintptr_t)0;
    ioapic_gsi_base = 0;
    ioapic_init_from_madt(madt);

    if (ioapic_base) {
        apic_route_device_irqs(madt);
        imcr_switch_to_apic_mode();
        kernel_log("Ioapic routing initialized.");
        kernel_log("Imcr switched to apic mode.");
    }
    else {
        kernel_log("No ioapic found device irqs will use legacy pic.");
    }

    apic_available = 1;
    kernel_log("Local apic initialized.");
}

void apic_send_end_of_interrupt(void) {
    if (apic_available) {
        apic_base[APIC_REG_EOI / 4] = 0;
    }
}

uint32_t apic_read_register(uint32_t reg_offset) {
    if (!apic_available) {
        return 0;
    }

    return apic_base[reg_offset / 4];
}

static uint32_t apic_calibrate_ticks_per_second(void) {
    const uint32_t calibration_ms = 10;
    uint16_t pit_reload = (uint16_t)((PIT_FREQUENCY / 1000) * calibration_ms);
    outb(PIT_COMMAND, 0x30);
    outb(PIT_CHANNEL0, pit_reload & 0xFF);
    outb(PIT_CHANNEL0, (pit_reload >> 8) & 0xFF);
    apic_base[APIC_REG_TIMER_INITIAL / 4] = 0xFFFFFFFF;
    uint16_t last_count = pit_reload;

    while (1) {
        outb(PIT_COMMAND, 0x00);
        uint8_t low = inb(PIT_CHANNEL0);
        uint8_t high = inb(PIT_CHANNEL0);
        uint16_t count = (uint16_t)(low | ((uint16_t)high << 8));

        if (count == 0 || count > last_count) {
            break;
        }

        last_count = count;
    }

    uint32_t apic_current = apic_base[APIC_REG_TIMER_CURRENT / 4];
    uint32_t apic_ticks_elapsed = 0xFFFFFFFF - apic_current;
    return (apic_ticks_elapsed / calibration_ms) * 1000;
}

void apic_timer_init(uint32_t hz) {
    if (!apic_available || !apic_base) {
        return;
    }

    apic_base[APIC_REG_TIMER_DIVIDE / 4] = 0xB;
    apic_base[APIC_REG_LVT_TIMER / 4] = 0x40 | APIC_LVT_MASKED;
    uint32_t ticks_per_second = apic_calibrate_ticks_per_second();

    if (ticks_per_second == 0) {
        kernel_log("Apic timer calibration failed.");
        kernel_log("Using fallback frequency");
        ticks_per_second = 10000000;
    }
    else {
        kernel_log("Apic timer calibrated to %u ticks per second.", ticks_per_second);
    }

    apic_base[APIC_REG_LVT_TIMER / 4] = 0x40 | APIC_TIMER_PERIODIC;
    apic_base[APIC_REG_TIMER_INITIAL / 4] = ticks_per_second / hz;
}

void apic_dump_redirection(uint32_t gsi) {
    uint32_t reg = IOAPIC_REDIRECTION0 + (gsi * 2);
    kernel_log("Gsi %u redir low %x high %x", gsi, ioapic_read(reg), ioapic_read(reg + 1));
}