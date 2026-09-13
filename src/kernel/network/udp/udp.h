#ifndef UDP_H
#define UDP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void udp_init(void);

void udp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]);

int udp_send(const uint8_t destination[4], uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t payload_length);

#ifdef __cplusplus
}
#endif

#endif // UDP_H