#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*KeyboardKeyCallback)(uint8_t scancode);

int ps2_keyboard_init(void);

void ps2_keyboard_interrupt(void);

void keyboard_process(void);

void keyboard_update(uint8_t scancode);

void keyboard_set_key_callback(KeyboardKeyCallback callback);

#ifdef __cplusplus
}
#endif

#endif // KEYBOARD_H