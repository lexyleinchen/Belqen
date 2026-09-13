#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void arp_init(void);

void arp_receive(const uint8_t* packet, uint16_t length);

int arp_resolve(const uint8_t ip[4], uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif // ARP_H