#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ETHERNET_HEADER_SIZE 14
#define ETHERNET_MIN_FRAME_SIZE 60
#define ETHERNET_MAX_FRAME_SIZE 1518
#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IPV6 0x86DD

struct ethernet_header {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t ether_type;
} __attribute__((packed));

void ethernet_init(void);

void ethernet_receive(const uint8_t* frame, uint16_t length);

int ethernet_send(const uint8_t destination[6], uint16_t ether_type, const uint8_t* payload, uint16_t payload_length);

void ethernet_get_mac(uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_H