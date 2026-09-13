#include "dhcp.h"
#include "../ipv4/ipv4.h"
#include "../../drivers/ethernet/ethernet.h"
#include "../../core/log.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_OP_BOOTREQUEST 1
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET 6
#define DHCP_DISCOVER 1
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS 6
#define DHCP_OPTION_REQUESTED_IP 50
#define DHCP_OPTION_SERVER_ID 54
#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_END 255
#define DHCP_MAGIC_COOKIE_0 0x63
#define DHCP_MAGIC_COOKIE_1 0x82
#define DHCP_MAGIC_COOKIE_2 0x53
#define DHCP_MAGIC_COOKIE_3 0x63

static uint32_t dhcp_transaction_id = 0x50524E54;
static uint8_t dhcp_offered_ip[4];
static uint8_t dhcp_server_ip[4];

static void dhcp_write_u32(uint8_t* buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)value;
}

static void dhcp_copy(uint8_t* destination, const uint8_t* source, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        destination[i] = source[i];
    }
}

void dhcp_init(void) {
    kernel_log("Dhcp initialized.");
}

static int dhcp_send_discover(void) {
    uint8_t packet[300];

    for (uint16_t i = 0; i < sizeof(packet); i++) {
        packet[i] = 0;
    }

    packet[0] = DHCP_OP_BOOTREQUEST;
    packet[1] = DHCP_HTYPE_ETHERNET;
    packet[2] = DHCP_HLEN_ETHERNET;
    packet[3] = 0;
    dhcp_write_u32(&packet[4], dhcp_transaction_id);
    packet[8] = 0;
    packet[9] = 0;
    packet[10] = 0x80;
    packet[11] = 0x00;
    uint8_t mac[6];
    ethernet_get_mac(mac);
    dhcp_copy(&packet[28], mac, 6);
    packet[236] = DHCP_MAGIC_COOKIE_0;
    packet[237] = DHCP_MAGIC_COOKIE_1;
    packet[238] = DHCP_MAGIC_COOKIE_2;
    packet[239] = DHCP_MAGIC_COOKIE_3;
    packet[240] = 53;
    packet[241] = 1;
    packet[242] = DHCP_DISCOVER;
    packet[243] = 55;
    packet[244] = 3;
    packet[245] = 1;
    packet[246] = 3;
    packet[247] = 6;
    packet[248] = 255;

    uint8_t source_ip[4] = {
        0, 0, 0, 0
    };

    uint8_t destionation_ip[4] = {
        255, 255, 255, 255
    };

    uint8_t destionation_mac[6] = {
        0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF
    };

    uint8_t udp_packet[308];
    uint16_t dhcp_length = 300;
    uint16_t udp_length = 8 + dhcp_length;
    udp_packet[0] = (uint8_t)(DHCP_CLIENT_PORT >> 8);
    udp_packet[1] = (uint8_t)(DHCP_CLIENT_PORT & 0xFF);
    udp_packet[2] = (uint8_t)(DHCP_SERVER_PORT >> 8);
    udp_packet[3] = (uint8_t)(DHCP_SERVER_PORT & 0xFF);
    udp_packet[4] = (uint8_t)(udp_length >> 8);
    udp_packet[5] = (uint8_t)(udp_length & 0xFF);
    udp_packet[6] = 0;
    udp_packet[7] = 0;
    dhcp_copy(&udp_packet[8], packet, dhcp_length);
    kernel_log("Dhcp sending discover...");
    return ipv4_send_raw(source_ip, destionation_ip, destionation_mac, 17, udp_packet, udp_length);
}

void dhcp_start(void) {
    kernel_log("Dhcp starting...");

    if (dhcp_send_discover()) {
        kernel_log("Dhcp discover sent.");
    }
    else {
        kernel_log("Dhcp discover failed.");
    }
}

static int dhcp_find_option(const uint8_t* packet, uint16_t length, uint8_t option_code, uint8_t* value, uint8_t value_size) {
    if (length < 240) {
        return 0;
    }

    uint16_t offset = 240;

    while (offset < length) {
        uint8_t code = packet[offset];

        if (code == DHCP_OPTION_END) {
            break;
        }

        if (code == 0) {
            offset++;
            continue;
        }

        if (offset + 1 >= length) {
            return 0;
        }

        uint8_t option_length = packet[offset + 1];

        if (offset + 2 + option_length > length) {
            return 0;
        }

        if (code == option_code) {
            if (option_length > value_size) {
                return 0;
            }

            for (uint8_t i = 0; i < option_length; i++) {
                value[i] = packet[offset + 2 + i];
            }

            return option_length;
        }

        offset += 2 + option_length;
    }

    return 0;
}

static int dhcp_send_request(void) {
    uint8_t packet[300];

    for (uint16_t i = 0; i < sizeof(packet); i++) {
        packet[i] = 0;
    }

    packet[0] = DHCP_OP_BOOTREQUEST;
    packet[1] = DHCP_HTYPE_ETHERNET;
    packet[2] = DHCP_HLEN_ETHERNET;
    packet[3] = 0;
    dhcp_write_u32(&packet[4], dhcp_transaction_id);
    packet[8] = 0;
    packet[9] = 0;
    packet[10] = 0x80;
    packet[11] = 0x00;
    uint8_t mac[6];
    ethernet_get_mac(mac);
    dhcp_copy(&packet[28], mac, 6);
    packet[236] = DHCP_MAGIC_COOKIE_0;
    packet[237] = DHCP_MAGIC_COOKIE_1;
    packet[238] = DHCP_MAGIC_COOKIE_2;
    packet[239] = DHCP_MAGIC_COOKIE_3;
    packet[240] = DHCP_OPTION_MESSAGE_TYPE;
    packet[241] = 1;
    packet[242] = DHCP_REQUEST;
    packet[243] = DHCP_OPTION_REQUESTED_IP;
    packet[244] = 4;
    
    for (uint8_t i = 0; i < 4; i++) {
        packet[245 + i] = dhcp_offered_ip[i];
    }

    packet[249] = DHCP_OPTION_SERVER_ID;
    packet[250] = 4;

    for (uint8_t i = 0; i < 4; i++) {
        packet[251 + i] = dhcp_server_ip[i];
    }

    packet[255] = 55;
    packet[256] = 3;
    packet[257] = DHCP_OPTION_SUBNET_MASK;
    packet[258] = DHCP_OPTION_ROUTER;
    packet[259] = DHCP_OPTION_DNS;
    packet[260] = DHCP_OPTION_END;

    uint8_t source_ip[4] = {
        0, 0, 0, 0
    };

    uint8_t destination_ip[4] = {
        255, 255, 255, 255
    };

    uint8_t destination_mac[6] = {
        0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF
    };

    uint8_t udp_packet[308];
    uint16_t dhcp_length = 300;
    uint16_t udp_length = 8 + dhcp_length;
    udp_packet[0] = (uint8_t)(DHCP_CLIENT_PORT >> 8);
    udp_packet[1] = (uint8_t)(DHCP_CLIENT_PORT & 0xFF);
    udp_packet[2] = (uint8_t)(DHCP_SERVER_PORT >> 8);
    udp_packet[3] = (uint8_t)(DHCP_SERVER_PORT & 0xFF);
    udp_packet[4] = (uint8_t)(udp_length >> 8);
    udp_packet[5] = (uint8_t)(udp_length & 0xFF);
    udp_packet[6] = 0;
    udp_packet[7] = 0;
    dhcp_copy(&udp_packet[8], packet, dhcp_length);
    kernel_log("Dhcp sending request for %u.%u.%u.%u", dhcp_offered_ip[0], dhcp_offered_ip[1], dhcp_offered_ip[2], dhcp_offered_ip[3]);
    return ipv4_send_raw(source_ip, destination_ip, destination_mac, 17, udp_packet, udp_length);
}

void dhcp_receive(const uint8_t* packet, uint16_t length, const uint8_t source[4], const uint8_t destination[4]) {
    kernel_log("Dhcp packet reveived length %u", length);

    if (length < 240) {
        kernel_log("Dhcp packet is too small.");
        return;
    }

    if (packet[0] != 2) {
        kernel_log("Dhcp packet is not a reply.");
        return;
    }

    uint32_t xid = ((uint32_t)packet[4] << 24) | ((uint32_t)packet[5] << 16) | ((uint32_t)packet[6] << 8) | packet[7];

    if (xid != dhcp_transaction_id) {
        kernel_log("Dhcp transaction id mismatch.");
        return;
    }

    for (uint8_t i = 0; i < 4; i++) {
        dhcp_offered_ip[i] = packet[16 + i];
    }

    kernel_log("Dhcp reply recieved offered ip %u.%u.%u.%u", dhcp_offered_ip[0], dhcp_offered_ip[1], dhcp_offered_ip[2], dhcp_offered_ip[3]);
    uint8_t message_type = 0;

    if (!dhcp_find_option(packet, length, DHCP_OPTION_MESSAGE_TYPE, &message_type, 1)) {
        kernel_log("Dhcp message type option missing.");
        return;
    }

    kernel_log("Dhcp message type %u", message_type);

    if (message_type == 2) {
        uint8_t server_ip[4];

        if (!dhcp_find_option(packet, length, DHCP_OPTION_SERVER_ID, server_ip, 4)) {
            kernel_log("Dhcp server identifier missing.");
            return;
        }

        for (uint8_t i = 0; i < 4; i++) {
            dhcp_server_ip[i] = server_ip[i];
        }

        kernel_log("Dhcp offer from server %u.%u.%u.%u", dhcp_server_ip[0], dhcp_server_ip[1], dhcp_server_ip[2], dhcp_server_ip[3]);

        if (dhcp_send_request()) {
            kernel_log("Dhcp request sent.");
        }
        else {
            kernel_log("Dhcp request failed.");
        }

        return;
    }
    else if (message_type == DHCP_ACK) {
        kernel_log("Dhcp ack received.");
        uint8_t netmask[4];
        uint8_t gateway[4];

        if (dhcp_find_option(packet, length, DHCP_OPTION_SUBNET_MASK, netmask, 4)) {
            ipv4_set_netmask(netmask);
        }
        else {
            kernel_log("Dhcp subnet mask missing.");
        }

        if (dhcp_find_option(packet, length, DHCP_OPTION_ROUTER, gateway, 4)) {
            ipv4_set_gateway(gateway);
        }
        else {
            kernel_log("Dhcp gateway missing.");
        }

        ipv4_set_address(dhcp_offered_ip);
        kernel_log("Dhcp configuration complete %u.%u.%u.%u", dhcp_offered_ip[0], dhcp_offered_ip[1], dhcp_offered_ip[2], dhcp_offered_ip[3]);
        return;
    }

    kernel_log("Dhcp unsupported message type %u", message_type);
}