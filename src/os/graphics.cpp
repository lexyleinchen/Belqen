#include "graphics.h"
#include "taskbar.h"

static Graphics graphics;

// Maximal supported resolution by the back buffer.
#define BACKBUFFER_WIDTH 1920
#define BACKBUFFER_HEIGHT 1080

static uint32_t backbuffer[BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT];

void graphics_init(Graphics new_graphics) {
    graphics = new_graphics;

    for (uint32_t y = 0 ; y < graphics.height; y++) {
        for (uint32_t x = 0; x < graphics.width; x++) {
            backbuffer[y * BACKBUFFER_WIDTH + x] = 0xFF000000;
        }
    }
}

void graphics_clear(uint32_t color) {
    if (graphics.framebuffer == nullptr) {
        return;
    }

    if (graphics.width > BACKBUFFER_WIDTH || graphics.height > BACKBUFFER_HEIGHT) {
        return;
    }

    for (uint32_t y = 0; y < graphics.height; y++) {
        for (uint32_t x = 0; x < graphics.width; x++) {
            backbuffer[y * BACKBUFFER_WIDTH + x] = color;
        }
    }
}

void graphics_rectangle(int x, int y, int width, int height, uint32_t color) {
    if (graphics.framebuffer == nullptr) {
        return;
    }

    uint8_t alpha = (color >> 24) & 0xFF;
    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    for (int yy = 0; yy < height; yy++) {
        for (int xx = 0; xx < width; xx++) {
            int px = x + xx;
            int py = y + yy;

            if (px < 0 || px >= static_cast<int>(graphics.width) || py < 0 || py >= static_cast<int>(graphics.height)) {
                continue;
            }

            uint32_t& destination = backbuffer[py * BACKBUFFER_WIDTH + px];
            
            if (alpha == 255) {
                destination = color;
                continue;
            }

            if (alpha == 0) {
                continue;
            }

            uint8_t dst_red = (destination >> 16) & 0xFF;
            uint8_t dst_green = (destination >> 8) & 0xFF;
            uint8_t dst_blue = destination & 0xFF;
            uint8_t out_red = (red * alpha + dst_red * (255 - alpha)) / 255;
            uint8_t out_green = (green * alpha + dst_green * (255 - alpha)) / 255;
            uint8_t out_blue = (blue * alpha + dst_blue * (255 - alpha)) / 255;
            destination = (0xFF << 24) | (out_red << 16) | (out_green << 8) | out_blue;
        }
    }
}

uint32_t graphics_width() {
    return graphics.width;
}

uint32_t graphics_height() {
    return graphics.height;
}

uint32_t desktop_height() {
    return graphics.height - TASKBAR_HEIGHT;
}

void graphics_present() {
    if (graphics.framebuffer == nullptr) {
        return;
    }

    if (graphics.width > BACKBUFFER_WIDTH || graphics.height > BACKBUFFER_HEIGHT) {
        return;
    }

    for (uint32_t y = 0; y < graphics.height; y++) {
        uint32_t* destination = graphics.framebuffer + (y * graphics.pitch / sizeof(uint32_t));
        uint32_t* source = backbuffer + (y * BACKBUFFER_WIDTH);

        for (uint32_t x = 0; x < graphics.width; x++) {
            destination[x] = source[x];
        }
    }
}