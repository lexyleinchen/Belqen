#ifndef IDE_H
#define IDE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ide_controller_found(uint16_t bus, uint8_t slot, uint8_t function, uint8_t prog_if);

void ide_init(void);

void ide_enable_interrupts(void);

void ide_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif // IDE_H