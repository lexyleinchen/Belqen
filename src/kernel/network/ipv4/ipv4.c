#include "ipv4.h"
#include "../../drivers/ethernet/ethernet.h"
#include "../arp/arp.h"
#include "../icmp/icmp.h"
#include "../udp/udp.h"
#include "../../core/log.h"

static struct ipv4_address ipv4_address = {
    {0, 0, 0, 0}
};

static struct ipv4_address ipv4_netmask = {
    {0, 0, 0, 0}
};

static struct ipv4_address ipv4_gateway = {
    {0, 0, 0, 0}
};

static uint16_t ipv4_swap16(uint16_t value) {
    return (uint16_t)(((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8));
}

uint16_t ipv4_checksum(const uint8_t* data, uint16_t length) {
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

void ipv4_get_address(uint8_t address[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        address[i] = ipv4_address.octet[i];
    }
}

void ipv4_set_address(const uint8_t address[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        ipv4_address.octet[i] = address[i];
    }

    kernel_log("Ipv4 address set to %u.%u.%u.%u", ipv4_address.octet[0], ipv4_address.octet[1], ipv4_address.octet[2], ipv4_address.octet[3]);
}

void ipv4_set_netmask(const uint8_t netmask[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        ipv4_netmask.octet[i] = netmask[i];
    }

    kernel_log("Ipv4 netmask set to %u.%u.%u.%u", ipv4_netmask.octet[0], ipv4_netmask.octet[1], ipv4_netmask.octet[2], ipv4_netmask.octet[3]);
}

void ipv4_set_gateway(const uint8_t gateway[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        ipv4_gateway.octet[i] = gateway[i];
    }

    kernel_log("Ipv4 gateway set to %u.%u.%u.%u", ipv4_gateway.octet[0], ipv4_gateway.octet[1], ipv4_gateway.octet[2], ipv4_gateway.octet[3]);
}

void ipv4_init(void) {
    kernel_log("Ipv4 initialized %u.%u.%u.%u", ipv4_address.octet[0], ipv4_address.octet[1], ipv4_address.octet[2], ipv4_address.octet[3]);
    kernel_log("Ipv4 netmask %u.%u.%u.%u", ipv4_netmask.octet[0], ipv4_netmask.octet[1], ipv4_netmask.octet[2], ipv4_netmask.octet[3]);
    kernel_log("Ipv4 gateway %u.%u.%u.%u", ipv4_gateway.octet[0], ipv4_gateway.octet[1], ipv4_gateway.octet[2], ipv4_gateway.octet[3]);
}

void ipv4_receive(const uint8_t* packet, uint16_t length) {
    if (length < 20) {
        kernel_log("Ipv4 packet too small.");
        return;
    }

    uint8_t version = packet[0] >> 4;
    uint8_t header_length = (packet[0] & 0x0F) * 4;

    if (version != 4) {
        kernel_log("Ipv4 invalid version.");
        return;
    }

    if (header_length < 20 || header_length > length) {
        kernel_log("Ipv4 invalid header length.");
        return;
    }

    uint16_t total_length = ((uint16_t)packet[2] << 8) | packet[3];

    if (total_length < header_length || total_length > length) {
        kernel_log("Ipv4 invalid total length.");
        return;
    }

    uint8_t protocol = packet[9];
    const uint8_t* source = &packet[12];
    const uint8_t* destination = &packet[16];
    kernel_log("Ipv4 %u.%u.%u.%u to %u.%u.%u.%u protocol %u", source[0], source[1], source[2], source[3], destination[0], destination[1], destination[2], destination[3], protocol);
    const uint8_t* payload = packet + header_length;
    uint16_t payload_length = total_length - header_length;

    if (protocol == 1) {
        icmp_receive(payload, payload_length, source, destination);
    }
    else if (protocol == 17) {
        udp_receive(payload, payload_length, source, destination);
    }
    else {
        kernel_log("Ipv4 unsupported protocol");
    }
}

int ipv4_send_raw(const uint8_t source[4], const uint8_t destination[4], const uint8_t destination_mac[6], uint8_t protocol, const uint8_t* payload, uint16_t payload_length) {
    if (payload_length > 1480) {
        kernel_log("Ipv4 raw payload is too large.");
        return 0;
    }

    uint8_t packet[1500];
    uint16_t total_length = 20 + payload_length;
    packet[0] = 0x45;
    packet[1] = 0;
    packet[2] = (uint8_t)(total_length >> 8);
    packet[3] = (uint8_t)(total_length & 0xFF);
    packet[4] = 0;
    packet[5] = 0;
    packet[6] = 0x40;
    packet[7] = 0;
    packet[8] = 64;
    packet[9] = protocol;
    packet[10] = 0;
    packet[11] = 0;

    for (uint32_t i = 0; i < 4; i++) {
        packet[12 + i] = source[i];
        packet[16 + i] = destination[i];
    }

    uint16_t checksum = ipv4_checksum(packet, 20);
    packet[10] = (uint8_t)(checksum >> 8);
    packet[11] = (uint8_t) (checksum & 0xFF); 

    for (uint16_t i = 0; i < payload_length; i++) {
        packet[20 + i] = payload[i];
    }

    kernel_log("Ipv4 raw send %u.%u.%u.%u to %u.%u.%u.%u", source[0], source[1], source[2], source[3], destination[0], destination[1], destination[2], destination[3]);

    return ethernet_send(destination_mac, ETHERTYPE_IPV4, packet, total_length);
}

int ipv4_send(const uint8_t destination[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_length) {
    if (payload_length > 1480) {
        kernel_log("Ipv4 payload is too large.");
        return 0;
    }

    uint8_t destination_mac[6];

    if (!arp_resolve(destination, destination_mac)) {
        kernel_log("Ipv4 no arp entry for %u.%u.%u.%u", destination[0], destination[1], destination[2], destination[3]);
        return 0;
    }

    uint8_t packet[1500];
    uint16_t total_length = 20 + payload_length;
    packet[0] = 0x45;
    packet[1] = 0;
    packet[2] = (uint8_t)(total_length >> 8);
    packet[3] = (uint8_t)(total_length & 0xFF);
    packet[4] = 0;
    packet[5] = 0;
    packet[6] = 0x40;
    packet[7] = 0;
    packet[8] = 64;
    packet[9] = protocol;
    packet[10] = 0;
    packet[11] = 0;

    for (uint32_t i = 0; i < 4; i++) {
        packet[12 + i] = ipv4_address.octet[i];
        packet[16 + i] = destination[i];
    }

    uint16_t checksum = ipv4_checksum(packet, 20);
    packet[10] = (uint8_t)(checksum >> 8);
    packet[11] = (uint8_t)(checksum & 0xFF);

    for (uint16_t i = 0; i < payload_length; i++) {
        packet[20 + i] = payload[i];
    }

    return ethernet_send(destination_mac, ETHERTYPE_IPV4, packet, total_length);
}