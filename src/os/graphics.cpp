#include "graphics.h"
#include "taskbar.h"
#include "../kernel/storage/vfs.h"
#include "../kernel/memory/malloc.h"

static Graphics graphics;

// Maximal supported resolution by the back buffer.
#define BACKBUFFER_WIDTH 1920
#define BACKBUFFER_HEIGHT 1080

static uint32_t backbuffer[BACKBUFFER_WIDTH * BACKBUFFER_HEIGHT];
static uint32_t* wallpaper_pixels;
static uint32_t wallpaper_width;
static uint32_t wallpaper_height;

static uint16_t bmp_u16(const uint8_t* data, uint32_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static uint32_t bmp_u32(const uint8_t* data, uint32_t offset) {
    return (uint32_t)data[offset] | ((uint32_t)data[offset + 1] << 8) | ((uint32_t)data[offset + 2] << 16) | ((uint32_t)data[offset + 3] << 24);
}

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

bool graphics_load_wallpaper(const char* path) {
    const uint64_t max_file_size = 64ULL * 1024 * 1024;
    FilesystemFile file;

    if (!path) {
        return false;
    }

    if (!vfs_open_path(path, &file)) {
        return false;
    }

    uint64_t file_size = file.size;

    if (file_size < 54 || file_size > max_file_size) {
        vfs_close(&file);
        return false;
    }

    uint8_t* data = (uint8_t*)malloc(file_size);
    
    if (!data) {
        vfs_close(&file);
        return false;
    }

    uint32_t total_bytes_read = 0;
    int read_ok = true;

    while (total_bytes_read < file_size) {
        uint32_t chunk_size = (uint32_t)(file_size - total_bytes_read);

        if (chunk_size > 4096) {
            chunk_size = 4096;
        }

        uint32_t chunk_bytes_read = 0;

        if (!vfs_read(&file, data + total_bytes_read, chunk_size, &chunk_bytes_read) || chunk_bytes_read != chunk_size) {
            read_ok = false;
            break;
        }

        total_bytes_read += chunk_bytes_read;
    }

    vfs_close(&file);

    if (!read_ok || total_bytes_read != file_size) {
        free(data);
        return false;
    }

    uint32_t dib_size = bmp_u32(data, 14);
    uint32_t pixel_offset = bmp_u32(data, 10);
    int32_t source_width = (int32_t)bmp_u32(data, 18);
    int32_t signed_height = (int32_t)bmp_u32(data, 22);

    if (data[0] != 'B' || data[1] != 'M' || dib_size < 40 || 14ULL + dib_size > file_size || pixel_offset < 14ULL + dib_size || pixel_offset > file_size || source_width <= 0 || signed_height == 0 || signed_height == INT32_MIN || bmp_u16(data, 26) != 1 || bmp_u16(data, 28) != 24 || bmp_u32(data, 30) != 0) {
        free(data);
        return false;
    }

    uint32_t source_height = signed_height < 0 ? (uint32_t)-signed_height : (uint32_t)signed_height;

    if ((uint32_t)source_width > 16384 || source_height > 16384 || graphics.width == 0 || graphics.height == 0 || graphics.width > BACKBUFFER_WIDTH || graphics.height > BACKBUFFER_HEIGHT) {
        free(data);
        return false;
    }

    uint64_t row_size = (((uint64_t)source_width * 3 + 3) / 4) * 4;
    uint64_t pixel_bytes = row_size * source_height;

    if (pixel_bytes > file_size - pixel_offset) {
        free(data);
        return false;
    }

    uint64_t output_count = (uint64_t)graphics.width * graphics.height;
    uint32_t* output = (uint32_t*)malloc(output_count * sizeof(uint32_t));

    if (!output) {
        free(data);
        return false;
    }

    bool top_down = signed_height < 0;

    for (uint32_t y = 0; y < graphics.height; y++) {
        uint32_t sampled_y = (uint64_t)y * source_height / graphics.height;
        uint32_t source_y = top_down ? sampled_y : source_height - 1 - sampled_y;
        const uint8_t* row = data + pixel_offset + (uint64_t)source_y * row_size;

        for (uint32_t x = 0; x < graphics.width; x++) {
            uint32_t source_x = (uint64_t)x * source_width / graphics.width;
            const uint8_t* pixel = row + source_x * 3;
            output[(uint64_t)y * graphics.width + x] = 0xFF000000 | ((uint32_t)pixel[2] << 16) | ((uint32_t)pixel[1] << 8) | pixel[0];
        }
    }

    free(data);
    free(wallpaper_pixels);
    wallpaper_pixels = output;
    wallpaper_width = graphics.width;
    wallpaper_height = graphics.height;
    return true;
}

bool graphics_draw_wallpaper() {
    if (!wallpaper_pixels || wallpaper_width != graphics.width || wallpaper_height != graphics.height || graphics.width > BACKBUFFER_WIDTH || graphics.height > BACKBUFFER_HEIGHT) {
        return false;
    }

    for (uint32_t y = 0; y < graphics.height; y++) {
        for (uint32_t x = 0; x < graphics.width; x++) {
            backbuffer[y * BACKBUFFER_WIDTH + x] = wallpaper_pixels[y * wallpaper_width + x];
        }
    }

    return true;
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
        uint32_t* destination = graphics.framebuffer + (static_cast<uintptr_t>(y) * graphics.pitch / sizeof(uint32_t));
        uint32_t* source = backbuffer + (static_cast<uintptr_t>(y) * BACKBUFFER_WIDTH);

        for (uint32_t x = 0; x < graphics.width; x++) {
            destination[x] = source[x];
        }
    }
}