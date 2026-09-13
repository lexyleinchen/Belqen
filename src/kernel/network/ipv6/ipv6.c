#include "ipv6.h"
#include "../../core/log.h"

static struct ipv6_address ipv6_link_local = {
    {
        0xFE, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01
    }
};

void ipv6_init(void) {
    kernel_log("Ipv6 initialized.");
    kernel_log("Ipv6 link-local address configured.");
}

void ipv6_receive(const uint8_t* packet, uint16_t length) {
    if (length < 40) {
        kernel_log("Ipv6 packet too small.");
        return;
    }

    uint8_t version = packet[0] >> 4;

    if (version != 6) {
        kernel_log("Ipv6 invalid version.");
        return;
    }

    uint8_t next_header = packet[6];
    kernel_log("Ipv6 packet received next header %u", next_header);
}