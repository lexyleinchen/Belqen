#include "arp.h"
#include "../../drivers/ethernet/ethernet.h"
#include "../ipv4/ipv4.h"
#include "../../core/log.h"

#define ARP_HARDWARE_ETHERNET 1
#define ARP_PROTOCOL_IPV4 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define ARP_CACHE_SIZE 16

struct arp_cache_entry {
    uint8_t ip[4];
    uint8_t mac[6];
    uint8_t valid;
};

static struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];

static void arp_copy_mac(uint8_t destination[6], const uint8_t source[6]) {
    for (uint32_t i = 0; i < 6; i++) {
        destination[i] = source[i];
    }
}

static void arp_copy_ip(uint8_t destination[4], const uint8_t source[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        destination[i] = source[i];
    }
}

static int arp_ip_equal(const uint8_t a[4], const uint8_t b[4]) {
    for (uint32_t i = 0; i < 4; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }

    return 1;
}

static void arp_cache_add(const uint8_t ip[4], const uint8_t mac[6]) {
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_ip_equal(arp_cache[i].ip, ip)) {
            arp_copy_mac(arp_cache[i].mac, mac);
            return;
        }
    }

    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_copy_ip(arp_cache[i].ip, ip);
            arp_copy_mac(arp_cache[i].mac, mac);
            arp_cache[i].valid = 1;
            return;
        }
    }

    kernel_log("Arp cache is full.");
}

static void arp_send_reply(const uint8_t destination_ip[4], const uint8_t destination_mac[6]) {
    uint8_t packet[28];
    packet[0] = 0x00;
    packet[1] = 0x01;
    packet[2] = 0x08;
    packet[3] = 0x00;
    packet[4] = 6;
    packet[5] = 4;
    packet[6] = 0x00;
    packet[7] = ARP_REPLY;
    uint8_t sender_mac[6];
    ethernet_get_mac(sender_mac);

    for (uint32_t i = 0; i < 6; i++) {
        packet[8 + i] = sender_mac[i];
    }

    uint8_t local_ip[4];
    ipv4_get_address(local_ip);

    for (uint32_t i = 0; i < 4; i++) {
        packet[14 + i] = local_ip[i];
    }

    for (uint32_t i = 0; i < 6; i++) {
        packet[18 + i] = destination_mac[i];
    }

    for (uint32_t i = 0; i < 4; i++) {
        packet[24 + i] = destination_ip[i];
    }

    ethernet_send(destination_mac, ETHERTYPE_ARP, packet, sizeof(packet));
    kernel_log("Arp reply sent.");
}

void arp_init(void) {
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }

    kernel_log("Arp initialized.");
}

void arp_receive(const uint8_t* packet, uint16_t length) {
    if (length < 28) {
        kernel_log("Arp packet too small.");
        return;
    }

    uint16_t hardware_type = ((uint16_t)packet[0] << 8) | packet[1];
    uint16_t protocol_type = ((uint16_t)packet[2] << 8) | packet[3];
    uint8_t hardware_length = packet[4];
    uint8_t protocol_length = packet[5];
    uint16_t operation = ((uint16_t)packet[6] << 8) | packet[7];
    kernel_log("Arp packet htype %u ptype %u op %u", hardware_type, protocol_type, operation);

    if (hardware_type != 1 || protocol_type != 0x0800 || hardware_length != 6 || protocol_length != 4) {
        kernel_log("Arp unsupported format.");
        return;
    }

    const uint8_t* sender_mac = &packet[8];
    const uint8_t* sender_ip = &packet[14];
    const uint8_t* target_mac = &packet[18];
    const uint8_t* target_ip = &packet[24];
    kernel_log("Arp %u %u.%u.%u.%u.", operation, sender_ip[0], sender_ip[1], sender_ip[2], sender_ip[3]);
    arp_cache_add(sender_ip, sender_mac);
    uint8_t local_ip[4];
    ipv4_get_address(local_ip);

    if (operation == ARP_REQUEST) {
        if (arp_ip_equal(target_ip, local_ip)) {
            kernel_log("Arp request received.");
            arp_send_reply(sender_ip, sender_mac);
        }
    }
    else if (operation == ARP_REPLY) {
        kernel_log("Arp reply received.");
    }
    else {
        kernel_log("Arp unknown operation.");
    }
}

int arp_resolve(const uint8_t ip[4], uint8_t mac[6]) {
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            continue;
        }

        if (!arp_ip_equal(arp_cache[i].ip, ip)) {
            continue;
        }

        arp_copy_mac(mac, arp_cache[i].mac);
        return 1;
    }

    return 0;
}