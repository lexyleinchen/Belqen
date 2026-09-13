#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void icmp_init(void);

void icmp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]);

#ifdef __cplusplus
}
#endif

#endif // ICMP_H