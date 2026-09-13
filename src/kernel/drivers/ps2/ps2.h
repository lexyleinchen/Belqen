#ifndef PS2_H
#define PS2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ps2_init(void);

void ps2_poll(void);

int ps2_mouse_write(uint8_t value);

int ps2_mouse_read(uint8_t* value);

int ps2_mouse_data_available(void);

int ps2_keyboard_write(uint8_t value);

int ps2_keyboard_read(uint8_t* value);

int ps2_keyboard_data_available(void);

#ifdef __cplusplus
}
#endif

#endif // PS2_H