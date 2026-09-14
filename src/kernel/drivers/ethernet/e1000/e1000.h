#ifndef E1000_H
#define E1000_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void e1000_controller_found(uint8_t bus, uint8_t slot, uint8_t function);

void e1000_poll(void);

void e1000_interrupt(void);

int e1000_send(const uint8_t* data, uint16_t length);

void e1000_get_mac(uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif // E1000_H