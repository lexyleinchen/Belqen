#include "mouse.h"
#include "../drivers/ps2/ps2.h"
#include "../core/log.h"

#define PS2_MOUSE_ACK 0xFA
#define PS2_MOUSE_RESEND 0xFE
#define PS2_MOUSE_RESET 0xFF
#define PS2_MOUSE_SET_DEFAULTS 0xF6
#define PS2_MOUSE_ENABLE 0xF4

static MouseState mouse_state;
static int32_t mouse_screen_width;
static int32_t mouse_screen_height;
static uint8_t ps2_mouse_packet[3];
static uint8_t ps2_mouse_packet_index = 0;
static uint8_t mouse_previous_buttons = 0;
static uint8_t mouse_clicked_buttons = 0;

void mouse_init(uint32_t screen_width, uint32_t screen_height) {
    mouse_screen_width = (int32_t)screen_width;
    mouse_screen_height = (int32_t)screen_height;
    mouse_state.x = mouse_screen_width / 2;
    mouse_state.y = mouse_screen_height / 2;
    mouse_state.buttons = 0;
}

void mouse_update(int8_t dx, int8_t dy, uint8_t buttons) {
    mouse_previous_buttons = mouse_state.buttons;
    mouse_state.x += dx;
    mouse_state.y += dy;
    mouse_state.buttons = buttons;
    mouse_clicked_buttons = buttons & ~mouse_previous_buttons;

    if (mouse_state.x < 0) {
        mouse_state.x = 0;
    }

    if (mouse_state.y < 0) {
        mouse_state.y = 0;
    }

    if (mouse_state.x >= mouse_screen_width) {
        mouse_state.x = mouse_screen_width - 1;
    }

    if (mouse_state.y >= mouse_screen_height) {
        mouse_state.y = mouse_screen_height - 1;
    }
}

void mouse_get_state(MouseState* state) {
    *state = mouse_state;
}

static int ps2_mouse_command(uint8_t command) {
    if (!ps2_mouse_write(command)) {
        kernel_log("ps2 mouse failed to send command.");
        return 0;
    }

    uint8_t response;

    if (!ps2_mouse_read(&response)) {
        kernel_log("ps2 mouse no command response.");
        return 0;
    }

    if (response == PS2_MOUSE_ACK) {
        return 1;
    }

    if (response == PS2_MOUSE_RESEND) {
        kernel_log("ps2 mouse device requested resend.");
        return 0;
    }

    kernel_log("ps2 mouse unexpected response %u", (uint32_t)response);
    return 0;
}

int ps2_mouse_init(void) {
    kernel_log("initializing ps2 mouse...");

    if (!ps2_mouse_write(PS2_MOUSE_RESET)) {
        kernel_log("ps2 mouse reset failed.");
        return 0;
    }

    uint8_t response;

    if (!ps2_mouse_read(&response)) {
        kernel_log("ps2 mouse reset response missing.");
        return 0;
    }

    if (response != PS2_MOUSE_ACK) {
        kernel_log("ps2 mouse reset was not acknowledged.");
        return 0;
    }

    if (!ps2_mouse_read(&response)) {
        kernel_log("ps2 mouse self-test response missing.");
        return 0;
    }

    if (response != 0xAA) {
        kernel_log("ps2 mouse self-test failed %u", (uint32_t)response);
        return 0;
    }

    if (!ps2_mouse_read(&response)) {
        kernel_log("ps2 mouse device id missing.");
        return 0;
    }

    kernel_log("ps2 mouse device id %u", (uint32_t)response);

    if (!ps2_mouse_command(PS2_MOUSE_SET_DEFAULTS)) {
        kernel_log("ps2 mouse failed to set defaults.");
        return 0;
    }

    if (!ps2_mouse_command(PS2_MOUSE_ENABLE)) {
        kernel_log("ps2 mouse failed to enable reporting.");
        return 0;
    }

    ps2_mouse_packet_index = 0;
    kernel_log("ps2 mouse initialized.");
    return 1;
}

static void ps2_mouse_process_packet(void) {
    uint8_t flags = ps2_mouse_packet[0];

    if ((flags & 0x08) == 0) {
        kernel_log("ps2 mouse invalid packet.");
        return;
    }

    if (flags & 0x40 || flags & 0x80) {
        return;
    }

    uint8_t buttons = flags & 0x07;
    int8_t dx = (int8_t)ps2_mouse_packet[1];
    int8_t dy = (int8_t)ps2_mouse_packet[2];
    dy = -dy;
    mouse_update(dx, dy, buttons);
}

void ps2_mouse_interrupt(void) {
    while (ps2_mouse_data_available()) {
        uint8_t data;

        if (!ps2_mouse_read(&data)) {
            break;
        }

        if (ps2_mouse_packet_index == 0) {
            if ((data & 0x08) == 0) {
                continue;;
            }
        }

        ps2_mouse_packet[ps2_mouse_packet_index] = data;
        ps2_mouse_packet_index++;

        if (ps2_mouse_packet_index >= 3) {
            ps2_mouse_process_packet();
            ps2_mouse_packet_index = 0;
        }
    }
}

int mouse_left_clicked(void) {
    if (mouse_clicked_buttons & 0x01) {
        mouse_clicked_buttons &= ~0x01;
        return 1;
    }
    
    return 0;
}

int mouse_left_pressed(void) {
    return (mouse_state.buttons & 0x01) != 0;
}

int mouse_right_clicked(void) {
    if (mouse_clicked_buttons & 0x02) {
        mouse_clicked_buttons &= ~0x02;
        return 1;
    }
    
    return 0;
}

int mouse_middle_clicked(void) {
    if (mouse_clicked_buttons & 0x04) {
        mouse_clicked_buttons &= ~0x04;
        return 1;
    }
    
    return 0;
}