#include "udp.h"
#include "../ipv4/ipv4.h"
#include "../dhcp/dhcp.h"
#include "../../core/log.h"

void udp_init(void) {
    kernel_log("Udp initialized.");
}

void udp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]) {
    if (length < 8) {
        kernel_log("Udp packet is too small.");
        return;
    }

    uint16_t source_port = ((uint16_t)packet[0] << 8) | packet[1];
    uint16_t destination_port = ((uint16_t)packet[2] << 8) | packet[3];
    uint16_t udp_length = ((uint16_t)packet[4] << 8) | packet[5];

    if (udp_length < 8 || udp_length > length) {
        kernel_log("Udp invalid length.");
        return;
    }

    kernel_log("Udp %u to %u length %u", source_port, destination_port, udp_length);

    const uint8_t* payload = packet + 8;
    uint16_t payload_length = udp_length - 8;

    if (destination_port == 68) {
        dhcp_receive(payload, payload_length, source, destination);
    }
    else {
        kernel_log("Udp unsupported destination port %u", destination_port);
    }
}

int udp_send(const uint8_t destination[4], uint16_t source_port, uint16_t destination_port, const uint8_t* payload, uint16_t payload_length) {
    if (payload_length > 1472) {
        kernel_log("Udp payload is too large.");
        return 0;
    }

    uint8_t packet[1500];
    uint16_t length = 8 + payload_length;
    packet[0] = (uint8_t)(source_port >> 8);
    packet[1] = (uint8_t)(source_port & 0xFF);
    packet[2] = (uint8_t)(destination_port >> 8);
    packet[3] = (uint8_t)(destination_port & 0xFF);
    packet[4] = (uint8_t)(length >> 8);
    packet[5] = (uint8_t)(length & 0xFF);
    packet[6] = 0;
    packet[7] = 0;

    for (uint16_t i = 0; i < payload_length; i++) {
        packet[8 + i] = payload[i];
    }

    return ipv4_send(destination, 17, packet, length);
} 