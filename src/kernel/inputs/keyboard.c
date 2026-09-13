#include "keyboard.h"
#include "../drivers/ps2/ps2.h"
#include "../core/log.h"

#define PS2_KEYBOARD_ACK 0xFA
#define PS2_KEYBOARD_RESEND 0xFE
#define PS2_KEYBOARD_RESET 0xFF
#define PS2_KEYBOARD_SET_DEFAULTS 0xF6
#define PS2_KEYBOARD_ENABLE 0xF4

static KeyboardKeyCallback keyboard_callback = 0;

void ps2_keyboard_poll(void) {
    while (ps2_keyboard_data_available()) {
        uint8_t scancode;

        if (!ps2_keyboard_read(&scancode)) {
            break;
        }

        keyboard_update(scancode);
    }
}

void keyboard_update(uint8_t scancode) {
    if (scancode & 0x80) {
        return;
    }

    kernel_log("keyboard scancode %x", scancode);

    if (keyboard_callback) {
        keyboard_callback(scancode);
    }
}

void keyboard_set_key_callback(KeyboardKeyCallback callback) {
    keyboard_callback = callback;
}

static int ps2_keyboard_command(uint8_t command) {
    if (!ps2_keyboard_write(command)) {
        kernel_log("ps2 keyboard failed to send command.");
        return 0;
    }

    uint8_t response;

    if (!ps2_keyboard_read(&response)) {
        kernel_log("ps2 keyboard no command response.");
        return 0;
    }

    if (response == PS2_KEYBOARD_ACK) {
        return 1;
    }

    if (response == PS2_KEYBOARD_RESEND) {
        kernel_log("ps2 keyboard requested resend.");
        return 0;
    }

    kernel_log("ps2 keyboard unexpected response %u", (uint32_t)response);
    return 0;
}

int ps2_keyboard_init(void) {
    kernel_log("initializing ps2 keyboard...");

    if (!ps2_keyboard_write(PS2_KEYBOARD_RESET)) {
        kernel_log("ps2 keyboard reset failed.");
        return 0;
    }

    uint8_t response;

    if (!ps2_keyboard_read(&response)) {
        kernel_log("ps2 keyboard reset response missing.");
        return 0;
    }

    if (response != PS2_KEYBOARD_ACK) {
        kernel_log("ps2 keyboard reset was not acknowledged.");
        return 0;
    }

    if (!ps2_keyboard_read(&response)) {
        kernel_log("ps2 keyboard self-test response missing.");
        return 0;
    }

    if (response != 0xAA) {
        kernel_log("ps2 keyboard self-test failed %u", (uint32_t)response);
        return 0;
    }

    if (!ps2_keyboard_command(PS2_KEYBOARD_SET_DEFAULTS)) {
        kernel_log("ps2 keyboard failed to set defaults.");
        return 0;
    }

    if (!ps2_keyboard_command(PS2_KEYBOARD_ENABLE)) {
        kernel_log("ps2 keyboard failed to enable reporting.");
        return 0;
    }

    kernel_log("ps2 keyboard initialized.");
    return 1;
}