#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MouseState {
    int32_t x;
    int32_t y;
    uint8_t buttons;
} MouseState;

void mouse_init(uint32_t screen_width, uint32_t screen_height);

void mouse_update(int8_t dx, int8_t dy, uint8_t buttons);

void mouse_get_state(MouseState* state);

int ps2_mouse_init(void);

void ps2_mouse_interrupt(void);

int mouse_left_clicked(void);

int mouse_left_pressed(void);

int mouse_right_clicked(void);

int mouse_middle_clicked(void);

#ifdef __cplusplus
}
#endif

#endif // MOUSE_H