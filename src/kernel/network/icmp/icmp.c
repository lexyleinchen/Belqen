#include "icmp.h"
#include "../ipv4/ipv4.h"
#include "../../core/log.h"

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

static uint16_t icmp_checksum(const uint8_t* data, uint16_t length) {
    uint32_t sum = 0;

    for (uint16_t i = 0; i + 1 < length; i += 2) {
        uint16_t word = ((uint16_t)data[i] << 8) | data[i + 1];
        sum += word;
    }

    if (length & 1) {
        sum += (uint16_t)data[length - 1] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

void icmp_init(void) {
    kernel_log("Icmp initialized.");
}

static void icmp_send_echo_reply(const uint8_t destination[4], const uint8_t* request, uint16_t length) {
    if (length < 8) {
        return;
    }

    uint8_t reply[1500];

    if (length > sizeof(reply)) {
        kernel_log("Icmp packet is too large.");
        return;
    }

    for (uint16_t i = 0; i < length; i++) {
        reply[i] = request[i];
    }

    reply[0] = ICMP_ECHO_REPLY;
    reply[1] = 0;
    reply[2] = 0;
    reply[3] = 0;
    uint16_t checksum = icmp_checksum(reply, length);
    reply[2] = (uint8_t)(checksum >> 8);
    reply[3] = (uint8_t)(checksum & 0xFF);

    if (ipv4_send(destination, 1, reply, length)) {
        kernel_log("Icmp echo reply sent.");
    }
    else {
        kernel_log("Icmp echo reply failed.");
    }
}

void icmp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]) {
    if (length < 8) {
        kernel_log("Icmp packet is too small.");
        return;
    }

    uint8_t type = packet[0];
    uint8_t code = packet[1];
    uint16_t received_checksum = ((uint16_t)packet[2] << 8) | packet[3];
    uint8_t checksum_packet[1500];

    if (length > sizeof(checksum_packet)) {
        return;
    }

    for (uint16_t i = 0; i < length; i++) {
        checksum_packet[i] = packet[i];
    }

    checksum_packet[2] = 0;
    checksum_packet[3] = 0;
    uint16_t calculated_checksum = icmp_checksum(checksum_packet, length);

    if (received_checksum != calculated_checksum) {
        kernel_log("Icmp invalid checksum.");
        return;
    }

    kernel_log("Icmp packet type %u code %u", type, code);

    if (type == ICMP_ECHO_REQUEST && code == 0) {
        kernel_log("Icmp echo request received.");
        icmp_send_echo_reply(source, packet, length);
    }
}