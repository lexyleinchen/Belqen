#include "os.h"
#include "graphics.h"
#include "taskbar.h"
#include "desktop.h"
#include "os_mouse.h"
#include "font.h"
#include "ui.h"

#include "../kernel/framebuffer/framebuffer.h"
#include "../kernel/core/log.h"
#include "../kernel/inputs/mouse.h"

extern "C"
void os_init(void) {
    kernel_log("initializing os...");

    // Initialize the framebuffer and graphics
    Framebuffer framebuffer = framebuffer_get();
    Graphics graphics;
    graphics.framebuffer = (uint32_t*)framebuffer.address;
    graphics.width = framebuffer.width;
    graphics.height = framebuffer.height;
    graphics.pitch = framebuffer.pitch;
    graphics_init(graphics);

    // OS initialization
    mouse_init(framebuffer.width, framebuffer.height);
    desktop_init();
    taskbar_init();

    kernel_log("os started.");
}

extern "C"
void os_draw(void) {
    graphics_clear(0xFF377c82); // Clear the screen with a color (blue)
    desktop_update();
    ui_update_windows();
    desktop_draw();
    ui_draw_windows();
    taskbar_draw();
    mouse_draw();
    graphics_present();
}