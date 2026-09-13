#include "desktop.h"
#include "app_registry.h"
#include "font.h"
#include "graphics.h"
#include "ui.h"

#include "../kernel/inputs/mouse.h"

#define DESKTOP_ICON_WIDTH 85
#define DESKTOP_ICON_HEIGHT 85
#define DESKTOP_ICON_SPACING_X 20
#define DESKTOP_ICON_SPACING_Y 20
#define DESKTOP_ICON_START_X 20
#define DESKTOP_ICON_START_Y 20
#define DESKTOP_DOUBLE_CLICK_FRAMES 100

static int last_clicked_app = -1;
static int double_click_timer = 0;

static void desktop_draw_icon(const App* app, int index) {
    if (app == nullptr) {
        return;
    }

    int x = DESKTOP_ICON_SPACING_X + index * (DESKTOP_ICON_WIDTH + DESKTOP_ICON_SPACING_X);
    int y = DESKTOP_ICON_SPACING_Y;
    MouseState mouse;
    mouse_get_state(&mouse);
    bool hovered = mouse.x >= x && mouse.x < x + DESKTOP_ICON_WIDTH && mouse.y >= y && mouse.y < y + DESKTOP_ICON_HEIGHT;
    uint32_t background_color = hovered && !ui_mouse_over_window(mouse.x, mouse.y) ? 0x60606060 : 0x00000000;
    graphics_rectangle(x, y, DESKTOP_ICON_WIDTH, DESKTOP_ICON_HEIGHT, background_color);
    int icon_width = 40;
    int icon_height = 40;
    int icon_x = x + (DESKTOP_ICON_WIDTH - icon_width) / 2;
    graphics_rectangle(icon_x, y + 10, 40, 40, 0xFF808080);
    const int max_char_per_line = 6;
    const char* name = app->name;
    char line1[max_char_per_line + 1];
    char line2[max_char_per_line + 2];
    int length = 0;

    while (name[length] != '\0' && length < max_char_per_line) {
        line1[length] = name[length];
        length++;
    }

    line1[length] = '\0';
    int remaining = 0;

    while (name[length + remaining] != '\0' && remaining < max_char_per_line) {
        line2[remaining] = name[length + remaining];
        remaining++;
    }
    line2[remaining] = '\0';

    if (name[length + remaining] != '\0' && remaining > 0) {
        line2[remaining - 1] = '.';
    }

    const int char_width = 13;
    int line1_width = length * char_width;
    int line2_width = remaining * char_width;
    int line1_x = x + (DESKTOP_ICON_WIDTH - line1_width) / 2;
    int line2_x = x + (DESKTOP_ICON_WIDTH - line2_width) / 2;
    font_draw_text(line1_x, y + 55, line1, 0xFFFFFFFF);
    font_draw_text(line2_x, y + 70, line2, 0xFFFFFFFF);
}

void desktop_init() {
    last_clicked_app = -1;
    double_click_timer = 0;
}

void desktop_update() {
    if (double_click_timer > 0) {
        double_click_timer--;

        if (double_click_timer == 0) {
            last_clicked_app = -1;
        }
    }

    MouseState mouse;
    mouse_get_state(&mouse);

    if (ui_mouse_over_window(mouse.x, mouse.y)) {
        return;
    }

    int count = app_count();

    for (int i = 0; i < count; i++) {
        int x = DESKTOP_ICON_SPACING_X + i * (DESKTOP_ICON_WIDTH + DESKTOP_ICON_SPACING_X);
        int y = DESKTOP_ICON_SPACING_Y;
        bool hovered = mouse.x >= x && mouse.x < x + DESKTOP_ICON_WIDTH && mouse.y >= y && mouse.y < y + DESKTOP_ICON_HEIGHT;

        if (!hovered) {
            continue;
        }

        if (!mouse_left_clicked()) {
            return;
        }

        if (last_clicked_app == i && double_click_timer > 0) {
            const App* app = app_get(i);
            ui_launch_app(app);
            last_clicked_app = -1;
            double_click_timer = 0;
            return;
        }

        last_clicked_app = i;
        double_click_timer = DESKTOP_DOUBLE_CLICK_FRAMES;
        return;
    }
}

void desktop_draw() {
    int count = app_count();

    for (int i = 0; i < count; i++) {
        desktop_draw_icon(app_get(i), i);
    }
}