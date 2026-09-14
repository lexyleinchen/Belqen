#ifndef APIC_H
#define APIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void apic_route_irq(uint8_t irq, uint8_t vector);

int apic_is_initialized(void);

int apic_is_available(void);

int apic_has_ioapic(void);

void apic_init(void);

void apic_send_end_of_interrupt(void);

uint32_t apic_read_register(uint32_t reg_offset);

void apic_timer_init(uint32_t hz);

void apic_dump_redirection(uint32_t gsi);

#ifdef __cplusplus
}
#endif

#endif // APIC_H