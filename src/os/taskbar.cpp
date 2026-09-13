#include "taskbar.h"
#include "graphics.h"
#include "ui.h"
#include "font.h"

#include "../kernel/network/ipv4/ipv4.h"

static bool start_menu_open = false;

void taskbar_init() {
    start_menu_open = false;
}

static void draw_start_menu() {
    int menu_width = 250;
    int button_height = 40;
    int spacing = 10;
    int screen_height = graphics_height();
    int menu_height = (button_height * 2) + (spacing * 2) + (15 * 2) + 10;
    int menu_x = 10;
    int menu_y = screen_height - TASKBAR_HEIGHT - menu_height;
    graphics_rectangle(menu_x, menu_y, menu_width, menu_height, 0xFF404040);
    font_draw_text(menu_x + (menu_width - ((7 * 10) + (6 * 4))) / 2, menu_y + 15, "PrintOS", 0xFFFFFFFF);

    if (graphics_button(menu_x + 15, menu_y + 15 + spacing + 10, menu_width - 30, button_height, 0xFF606060, "Restart", 0xFFFFFFFF)) {
        // restart
    }

    if (graphics_button(menu_x + 15, menu_y + 15 + spacing * 2 + button_height + 10, menu_width - 30, button_height, 0xFF606060, "Shutdown", 0xFFFFFFFF)) {
        // shutdown
    }
}

static void ip_to_string(char* buffer, const uint8_t ip[4]) {
    uint32_t pos = 0;

    for (uint32_t i = 0; i < 4; i++) {
        uint8_t value = ip[i];

        if (value >= 100) {
            buffer[pos++] = '0' + (value / 100);
            value %= 100;
            buffer[pos++] = '0' + (value / 10);
            value %= 10;
            buffer[pos++] = '0' + value;
        }
        else if (value >= 10) {
            buffer[pos++] = '0' + (value / 10);
            value %= 10;
            buffer[pos++] = '0' + value;
        }
        else {
            buffer[pos++] = '0' + value;
        }

        if (i < 3) {
            buffer[pos++] = '.';
        }
    }
    
    buffer[pos] = '\0';
}

void taskbar_draw() {
    int screen_width = graphics_width();
    int screen_height = graphics_height();
    graphics_rectangle(0, screen_height - TASKBAR_HEIGHT, screen_width, TASKBAR_HEIGHT, 0xFF808080); // Draw the taskbar background (gray)
    int start_x = 10;
    int start_y = screen_height - TASKBAR_HEIGHT + 10;
    uint8_t ip[4];
    char ip_text[16];
    ipv4_get_address(ip);
    ip_to_string(ip_text, ip);
    uint32_t ip_length = 0;

    while (ip_text[ip_length] != '\0') {
        ip_length++;
    }
    
    uint32_t ip_width = ip_length * 14;
    font_draw_text(screen_width - ip_width - 20, screen_height - TASKBAR_HEIGHT + 18, ip_text, 0xFFFFFFFF);
    
    if (graphics_button(start_x, start_y, 100, 30, 0xFF606060, "Start", 0xFFFFFFFF)) {
        start_menu_open = !start_menu_open;
    }

    if (start_menu_open) {
        draw_start_menu();
    }
}