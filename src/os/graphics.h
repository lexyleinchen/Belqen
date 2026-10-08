#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

struct Graphics {
    uint32_t* framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

void graphics_init(Graphics graphics);

void graphics_clear(uint32_t color);

void graphics_rectangle(int x, int y, int width, int height, uint32_t color);

bool graphics_load_wallpaper(const char* path);

bool graphics_draw_wallpaper();

uint32_t graphics_width();

uint32_t graphics_height();

uint32_t desktop_height();

void graphics_present();

#endif // GRAPHICS_H