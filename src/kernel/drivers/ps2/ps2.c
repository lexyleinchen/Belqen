#include "ps2.h"
#include "../../inputs/keyboard.h"
#include "../../inputs/mouse.h"
#include "../../interrupts/interrupts.h"
#include "../../core/log.h"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_AUX_DATA 0x20
#define PS2_COMMAND_READ_CONFIG 0x20
#define PS2_COMMAND_WRITE_CONFIG 0x60
#define PS2_COMMAND_DISABLE_MOUSE 0xA7
#define PS2_COMMAND_ENABLE_MOUSE 0xA8
#define PS2_COMMAND_WRITE_MOUSE 0xD4
#define PS2_COMMAND_ENABLE_KEYBOARD 0xAE
#define PS2_COMMAND_DISABLE_KEYBOARD 0xAD
#define PS2_CONFIG_MOUSE_IRQ 0x02
#define PS2_CONFIG_MOUSE_CLOCK 0x20
#define PS2_CONFIG_KEYBOARD_IRQ 0x01
#define PS2_CONFIG_KEYBOARD_TRANSLATION 0x40

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int ps2_wait_input_clear(void) {
    for (uint32_t timeout = 0; timeout < 100000; timeout++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
            return 1;
        }
    }

    kernel_log("ps2 timeout waiting for input buffer.");
    return 0;
}

static void ps2_flush_output(void) {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        (void)inb(PS2_DATA_PORT);
    }
}

static int ps2_write_command(uint8_t command) {
    if (!ps2_wait_input_clear()) {
        return 0;
    }

    outb(PS2_COMMAND_PORT, command);
    return 1;
}

static int ps2_write_data(uint8_t value) {
    if (!ps2_wait_input_clear()) {
        return 0;
    }

    outb(PS2_DATA_PORT, value);
    return 1;
}

static int ps2_read_data(uint8_t* value) {
    for (uint32_t timeout = 0; timeout < 100000; timeout++) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            *value = inb(PS2_DATA_PORT);
            return 1;
        }
    }

    kernel_log("ps2 timeout waiting for output buffer.");
    return 0;
}

int ps2_mouse_write(uint8_t value) {
    if (!ps2_write_command(PS2_COMMAND_WRITE_MOUSE)) {
        return 0;
    }

    if (!ps2_write_data(value)) {
        return 0;
    }

    return 1;
}

int ps2_mouse_read(uint8_t* value) {
    return ps2_read_data(value);
}

int ps2_mouse_data_available(void) {
    uint8_t status = inb(PS2_STATUS_PORT);

    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
        return 0;
    }

    if ((status & PS2_STATUS_AUX_DATA) == 0) {
        return 0;
    }

    return 1;
}

int ps2_keyboard_write(uint8_t value) {
    return ps2_write_data(value);
}

int ps2_keyboard_read(uint8_t* value) {
    return ps2_read_data(value);
}

int ps2_keyboard_data_available(void) {
    uint8_t status = inb(PS2_STATUS_PORT);

    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
        return 0;
    }

    if (status & PS2_STATUS_AUX_DATA) {
        return 0;
    }

    return 1;
}

static int ps2_read_config(uint8_t* config) {
    if (!ps2_write_command(PS2_COMMAND_READ_CONFIG)) {
        return 0;
    }

    return ps2_read_data(config);
}

static int ps2_write_config(uint8_t config) {
    if (!ps2_write_command(PS2_COMMAND_WRITE_CONFIG)) {
        return 0;
    }

    return ps2_write_data(config);
}

static int ps2_controller_init(void) {
    kernel_log("initializing ps2 controller...");

    if (!ps2_write_command(PS2_COMMAND_DISABLE_MOUSE)) {
        return 0;
    }

    if (!ps2_write_command(PS2_COMMAND_DISABLE_KEYBOARD)) {
        return 0;
    }

    ps2_flush_output();
    uint8_t config;

    if (!ps2_read_config(&config)) {
        kernel_log("ps2 failed to read controller configuration.");
        return 0;
    }

    config &= ~(PS2_CONFIG_MOUSE_IRQ);
    config |= PS2_CONFIG_KEYBOARD_IRQ;
    config &= ~(PS2_CONFIG_KEYBOARD_TRANSLATION);

    if (!ps2_write_config(config)) {
        kernel_log("ps2 failed to write controller configuration.");
        return 0;
    }

    if (!ps2_write_command(PS2_COMMAND_ENABLE_KEYBOARD)) {
        kernel_log("ps2 failed to enable keyboard.");
        return 0;
    }

    if (!ps2_write_command(PS2_COMMAND_ENABLE_MOUSE)) {
        kernel_log("ps2 failed to enable mouse.");
        return 0;
    }

    if (!ps2_read_config(&config)) {
        kernel_log("ps2 failed to read controller configuration.");
        return 0;
    }

    config &= ~PS2_CONFIG_MOUSE_CLOCK;
    config |= PS2_CONFIG_MOUSE_IRQ;

    if (!ps2_write_config(config)) {
        kernel_log("ps2 failed to enable mouse clock.");
        return 0;
    }

    ps2_flush_output();
    kernel_log("ps controller initialized.");
    return 1;
}

void ps2_init(void) {
    if (!ps2_controller_init()) {
        kernel_log("ps2 controller initialization failed.");
        return;
    }

    if (!ps2_keyboard_init()) {
        kernel_log("ps2 keyboard initialization failed.");
        return;
    }

    if (!ps2_mouse_init()) {
        kernel_log("ps2 mouse initialization failed.");
        return;
    }

    interrupt_register_irq(1, ps2_keyboard_interrupt);
    interrupt_unmask_irq(1);
    interrupt_register_irq(12, ps2_mouse_interrupt);
    interrupt_unmask_irq(12);

    kernel_log("ps2 initialized.");
}