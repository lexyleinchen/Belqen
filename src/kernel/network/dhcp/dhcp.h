#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dhcp_init(void);

void dhcp_start(void);

void dhcp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]);

#ifdef __cplusplus
}
#endif

#endif // DHCP_H