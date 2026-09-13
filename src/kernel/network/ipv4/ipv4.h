#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ipv4_address {
    uint8_t octet[4];
};

void ipv4_init(void);

void ipv4_receive(const uint8_t* packet, uint16_t length);

uint16_t ipv4_checksum(const uint8_t* data, uint16_t length);

int ipv4_send_raw(const uint8_t source[4], const uint8_t destination[4], const uint8_t destination_mac[6], uint8_t protocol, const uint8_t* payload, uint16_t payload_length);

int ipv4_send(const uint8_t destination[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_length);

void ipv4_get_address(uint8_t address[4]);

void ipv4_set_address(const uint8_t address[4]);

void ipv4_set_netmask(const uint8_t netmask[4]);

void ipv4_set_gateway(const uint8_t gateway[4]);

#ifdef __cplusplus
}
#endif

#endif // IPV4_H