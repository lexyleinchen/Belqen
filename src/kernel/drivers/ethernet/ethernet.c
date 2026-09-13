#include "ethernet.h"
#include "e1000/e1000.h"
#include "../../network/arp/arp.h"
#include "../../network/ipv4/ipv4.h"
#include "../../network/ipv6/ipv6.h"
#include "../../core/log.h"

static uint8_t ethernet_mac[6];

static uint16_t ethernet_swap16(uint16_t value) {
    return (uint16_t)(((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8));
}

static void ethernet_test(void) {
    uint8_t destination[6] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    uint8_t payload[] = {
        'P', 'r', 'i', 'n', 't', 'O', 'S'
    };

    kernel_log("Ethernet test sending...");

    if (ethernet_send(destination, 0x88B5, payload, sizeof(payload))) {
        kernel_log("Ethernet test frame queued.");
    }
    else {
        kernel_log("Ethernet test frame failed.");
    }
}

void ethernet_init(void) {
    e1000_get_mac(ethernet_mac);
    kernel_log("Ethernet initialized.");
    kernel_log("Ethernet mac %x:%x:%x:%x:%x:%x", ethernet_mac[0], ethernet_mac[1], ethernet_mac[2], ethernet_mac[3], ethernet_mac[4], ethernet_mac[5]);
}

int ethernet_send(const uint8_t destination[6], uint16_t ether_type, const uint8_t* payload, uint16_t payload_length) {
    if (payload_length > ETHERNET_MAX_FRAME_SIZE - ETHERNET_HEADER_SIZE) {
        kernel_log("Ethernet payload too large.");
        return 0;
    }

    uint8_t frame[ETHERNET_MAX_FRAME_SIZE];
    struct ethernet_header* header = (struct ethernet_header*)frame;

    for (uint32_t i = 0; i < 6; i++) {
        header->destination[i] = destination[i];
        header->source[i] = ethernet_mac[i];
    }

    header->ether_type = ethernet_swap16(ether_type);

    for (uint16_t i = 0; i < payload_length; i++) {
        frame[ETHERNET_HEADER_SIZE + i] = payload[i];
    }

    return e1000_send(frame, ETHERNET_HEADER_SIZE + payload_length);
}

void ethernet_receive(const uint8_t* frame, uint16_t length) {
    if (length < ETHERNET_HEADER_SIZE) {
        kernel_log("Ethernet frame too small.");
        return;
    }

    const struct ethernet_header* header = (const struct ethernet_header*)frame;
    uint16_t ether_type = ethernet_swap16(header->ether_type);
    kernel_log("Ethernet received frame type %u", ether_type);
    const uint8_t* payload = frame + ETHERNET_HEADER_SIZE;
    uint16_t payload_length = length - ETHERNET_HEADER_SIZE;
    (void)payload;
    (void)payload_length;

    if (ether_type == ETHERTYPE_IPV4) {
        kernel_log("Ethernet ipv4");
        ipv4_receive(payload, payload_length);
    }
    else if (ether_type == ETHERTYPE_ARP) {
        kernel_log("Ethernet arp");
        arp_receive(payload, payload_length);
    }
    else if (ether_type == ETHERTYPE_IPV6) {
        kernel_log("Ethernet ipv6");
        ipv6_receive(payload, payload_length);
    }
    else {
        kernel_log("Ethernet unknown ethertype.");
    }
}

void ethernet_get_mac(uint8_t mac[6]) {
    for (uint32_t i = 0; i < 6; i++) {
        mac[i] = ethernet_mac[i];
    }
}