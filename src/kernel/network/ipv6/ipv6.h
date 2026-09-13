#ifndef IPV6_H
#define IPV6_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ipv6_address {
    uint8_t bytes[16];
};

void ipv6_init(void);

void ipv6_receive(const uint8_t* packet, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif // IPV6_H