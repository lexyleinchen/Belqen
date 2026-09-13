#include "ui.h"
#include "graphics.h"
#include "font.h"

#include "../kernel/inputs/mouse.h"

#define WINDOW_TITLE_BAR_HEIGHT 26
#define WINDOW_CLOSE_BUTTON_WIDTH 24
#define WINDOW_CLOSE_BUTTON_HEIGHT 20
#define WINDOW_BORDER_SIZE 1
#define MAX_WINDOWS 32
#define SCROLLBAR_MIN_HEIGHT 20

static Window* windows[MAX_WINDOWS];
static int window_count;
static Window* focused_window = nullptr;
static Window window_pool[MAX_WINDOWS];
static bool window_used[MAX_WINDOWS];

bool graphics_button(int x, int y, int width, int height, uint32_t color, const char* text, uint32_t text_color) {
    MouseState mouse;
    mouse_get_state(&mouse);
    bool hoverd = mouse.x >= x && mouse.x < x + width && mouse.y >= y && mouse.y < y + height;
    graphics_rectangle(x, y, width, height, color);
    int text_width = 0;
    int character_count = 0;
    
    for (const char* character = text; *character != '\0'; character++) {
        character_count++;
    }

    if (character_count > 1) {
        text_width = character_count * 10 + (character_count - 1) * 4;
    }
    else if (character_count == 1) {
        text_width = 10;
    }

    int text_x = x + (width - text_width) / 2;
    int text_y = y + (height - 10) / 2;
    font_draw_text(text_x, text_y, text, text_color);
    return hoverd && mouse_left_clicked();
}

static Window* ui_alloc_window() {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!window_used[i]) {
            window_used[i] = true;
            return &window_pool[i];
        }
    }

    return nullptr;
}

static void ui_free_window(Window* window) {
    if (window == nullptr) {
        return;
    }

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (&window_pool[i] == window) {
            window_used[i] = false;
            return;
        }
    }
}

Window* ui_create_window(int x, int y, int width, int height, const char* title,uint32_t background_color, uint32_t title_bar_color, uint32_t title_color, WindowUpdateCallback update_content, WindowDrawCallback draw_content) {
    Window* window = ui_alloc_window();

    if (window == nullptr) {
        return nullptr;
    }

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->title = title;
    window->dragging = false;
    window->drag_offset_x = 0;
    window->drag_offset_y = 0;
    window->scrollbar.dragging = false;
    window->scrollbar.drag_offset = 0;
    window->background_color = background_color;
    window->title_bar_color = title_bar_color;
    window->title_color = title_color;
    window->update_content = update_content;
    window->draw_content = draw_content;
    window->app = nullptr;
    window->close_requested = false;
    return window;
}

void ui_register_window(Window* window) {
    if (window == nullptr) {
        return;
    }

    if (window_count >= MAX_WINDOWS) {
        return;
    }

    windows[window_count] = window;
    window_count++;
}

static Window* ui_get_window_at(int mouse_x, int mouse_y) {
    for (int i = window_count - 1; i >= 0; i--) {
        Window* window = windows[i];

        if (mouse_x >= window->x && mouse_x < window->x + window->width && mouse_y >= window->y && mouse_y < window->y + window->height) {
            return window;
        }
    }

    return nullptr;
}

bool ui_mouse_over_window(int mouse_x, int mouse_y) {
    return ui_get_window_at(mouse_x, mouse_y) != nullptr;
}

static Window* ui_find_app(const App* app) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i] != nullptr && windows[i]->app == app) {
            return windows[i];
        }
    }

    return nullptr;
}

static void ui_bring_to_front(Window* window) {
    if (window == nullptr) {
        return;
    }

    int index = -1;

    for (int i = 0; i < window_count; i++) {
        if (windows[i] == window) {
            index = i;
            break;
        }
    }

    if (index == -1 || index == window_count - 1) {
        return;
    }

    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }

    windows[window_count - 1] = window;
}

static void ui_focus_window(Window* window) {
    if (window == nullptr) {
        return;
    }

    ui_bring_to_front(window);
    focused_window = window;
}

Window* ui_launch_app(const App* app) {
    if (app == nullptr || app->init == nullptr) {
        return nullptr;
    }

    Window* existing = ui_find_app(app);

    if (existing != nullptr) {
        ui_focus_window(existing);
        return existing;
    }

    Window* window = app->init();

    if (window == nullptr) {
        return nullptr;
    }

    window->app = app;
    ui_register_window(window);
    ui_focus_window(window);
    return window;
}

static void ui_remove_window(Window* window) {
    if (window == nullptr) {
        return;
    }

    int index = -1;

    for (int i = 0; i < window_count; i++) {
        if (windows[i] == window) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return;
    }

    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }

    windows[window_count - 1] = nullptr;
    window_count--;

    if (focused_window == window) {
        focused_window = nullptr;

        if (window_count > 0) {
            focused_window = windows[window_count - 1];
        }
    }

    ui_free_window(window);
}

void ui_update_window(Window* window, bool active) {
    if (window == nullptr) {
        return;
    }

    MouseState mouse;
    mouse_get_state(&mouse);
    int x = window->x + window->width - WINDOW_CLOSE_BUTTON_WIDTH - 2;
    int y = window->y + 3;
    bool mouse_in_close_button = mouse.x >= x && mouse.x < x + WINDOW_CLOSE_BUTTON_WIDTH && mouse.y >= y && mouse.y < y + WINDOW_CLOSE_BUTTON_HEIGHT;

    if (active && mouse_in_close_button && mouse_left_clicked()) {
        window->close_requested = true;
        return;
    }

    bool mouse_left_down = mouse_left_pressed();
    bool mouse_in_title_bar = mouse.x >= window->x && mouse.x < window->x + window->width && mouse.y >= window->y && mouse.y < window->y + WINDOW_TITLE_BAR_HEIGHT;

    if (active) {
        if (!window->dragging && mouse_left_down && mouse_in_title_bar && !mouse_in_close_button) {
            window->dragging = true;
            window->drag_offset_x = mouse.x - window->x;
            window->drag_offset_y = mouse.y - window->y;
        }
    }

    if (window->dragging) {
        if (mouse_left_down) {
            window->x = mouse.x - window->drag_offset_x;
            window->y = mouse.y - window->drag_offset_y;
        }
        else {
            window->dragging = false;
        }
    }

    int desktop_width = graphics_width();
    int desktop_height_value = desktop_height();

    if (window->x < 0) {
        window->x = 0;
    }

    if (window->y < 0) {
        window->y = 0;
    }

    if (window->x + window->width > desktop_width) {
        window->x = desktop_width - window->width;
    }

    if (window->y + window->height > desktop_height_value) {
        window->y = desktop_height_value - window->height;
    }

    // Safety if window is bigger than desktop
    if (window->x < 0) {
        window->x = 0;
    }

    if (window->y < 0) {
        window->y = 0;
    }
}

void ui_update_windows() {
    MouseState mouse;
    mouse_get_state(&mouse);

    if (focused_window != nullptr && focused_window->scrollbar.dragging) {
        ui_update_window(focused_window, true);
    }
    else if (focused_window != nullptr && focused_window->dragging) {
        ui_update_window(focused_window, true);
    }
    else {
        Window* active_window = ui_get_window_at(mouse.x, mouse.y);

        if (active_window != nullptr && mouse_left_pressed()) {
            focused_window = active_window;
            ui_bring_to_front(active_window);
            ui_update_window(focused_window, true);
        }
    }

    for (int i = 0; i < window_count; i++) {
        Window* window = windows[i];

        if (window == nullptr) {
            continue;
        }

        if (window->update_content != nullptr) {
            window->update_content(window);
        }
    }

    for (int i = window_count - 1; i >= 0; i--) {
        Window* window = windows[i];

        if (window != nullptr && window->close_requested) {
            ui_remove_window(window);
        }
    }
}

void ui_draw_window(Window* window) {
    if (window == nullptr) {
        return;
    }

    graphics_rectangle(window->x, window->y, window->width, window->height, window->background_color);
    graphics_rectangle(window->x, window->y, window->width, WINDOW_TITLE_BAR_HEIGHT, window->title_bar_color);

    if (window->title != nullptr) {
        font_draw_text(window->x + 10, window->y + 8, window->title, window->title_color);
    }

    int close_x = window->x + window->width - WINDOW_CLOSE_BUTTON_WIDTH - 3;
    int close_y = window->y + 3;
    graphics_rectangle(close_x, close_y, WINDOW_CLOSE_BUTTON_WIDTH, WINDOW_CLOSE_BUTTON_HEIGHT, 0xFFC04040);
    font_draw_text(close_x + 7, close_y + 5, "X", 0xFFFFFFFF);
}

void ui_draw_windows() {
    for (int i = 0; i < window_count; i++) {
        Window* window = windows[i];

        if (window == nullptr) {
            continue;
        }

        ui_draw_window(window);

        if (window->draw_content != nullptr) {
            window->draw_content(window);
        }
    }
}

int ui_window_content_x(const Window* window) {
    return window->x;
}

int ui_window_content_y(const Window* window) {
    return window->y + WINDOW_TITLE_BAR_HEIGHT;
}

int ui_window_content_width(const Window* window) {
    return window->width;
}

int ui_window_content_height(const Window* window) {
    return window->height - WINDOW_TITLE_BAR_HEIGHT;
}

void draw_scrollbar(int x, int y, int width, int height, int total_lines, int visible_lines, int scroll) {
    if (total_lines <= visible_lines) {
        return;
    }

    graphics_rectangle(x, y, width, height, 0xFF202020);

    int thumb_height = (visible_lines * height) / total_lines;

    if (thumb_height < SCROLLBAR_MIN_HEIGHT) {
        thumb_height = SCROLLBAR_MIN_HEIGHT;
    }

    if (thumb_height > height) {
        thumb_height = height;
    }

    int max_scroll = total_lines - visible_lines;
    int max_thumb_y = height - thumb_height;
    int thumb_y = y;

    if (max_scroll > 0) {
        thumb_y += (scroll * max_thumb_y) / max_scroll;
    }

    graphics_rectangle(x + 2, thumb_y, width - 4, thumb_height, 0xFF606060);
}

void update_scrollbar(Window* window, int x, int y , int width, int height, int total_lines, int visible_lines, int& scroll) {
    if (total_lines <= visible_lines) {
        window->scrollbar.dragging = false;
        scroll = 0;
        return;
    }

    MouseState mouse;
    mouse_get_state(&mouse);
    int max_scroll = total_lines - visible_lines;
    int thumb_height = (visible_lines * height) / total_lines;

    if (thumb_height < SCROLLBAR_MIN_HEIGHT) {
        thumb_height = SCROLLBAR_MIN_HEIGHT;
    }

    if (thumb_height > height) {
        thumb_height = height;
    }

    int max_thumb_y = height - thumb_height;
    int thumb_y = y;

    if (max_scroll > 0) {
        thumb_y += (scroll * max_thumb_y) / max_scroll;
    }

    bool mouse_in_thumb = mouse.x >= x && mouse.x < x + width && mouse.y >= thumb_y && mouse.y < thumb_y + thumb_height;
    bool mouse_in_scrollbar = mouse.x >= x && mouse.x < x + width && mouse.y >= y && mouse.y < y + height;

    if (!window->scrollbar.dragging && mouse_left_pressed() && mouse_in_thumb) {
        window->scrollbar.dragging = true;
        window->scrollbar.drag_offset = mouse.y - thumb_y;
    }

    if (window->scrollbar.dragging) {
        if (!mouse_left_pressed()) {
            window->scrollbar.dragging = false;
            return;
        }

        int new_thumb_y = mouse.y - window->scrollbar.drag_offset;

        if (new_thumb_y < y) {
            new_thumb_y = y;
        }

        if (new_thumb_y > y + max_thumb_y) {
            new_thumb_y = y + max_thumb_y;
        }

        if (max_thumb_y > 0) {
            scroll = ((new_thumb_y - y) * max_scroll) / max_thumb_y;
        }

        if (scroll < 0) {
            scroll = 0;
        }

        if (scroll > max_scroll) {
            scroll = max_scroll;
        }

        return;
    }

    if (mouse_in_scrollbar && !mouse_in_thumb && mouse_left_clicked()) {
        if (mouse.y < thumb_y) {
            scroll -= visible_lines;
        }
        else {
            scroll += visible_lines;
        }

        if (scroll < 0) {
            scroll = 0;
        }

        if (scroll > max_scroll) {
            scroll = max_scroll;
        }
    }
}

void font_draw_progressbar(int x, int y, int width, int height, int filled, uint32_t background_color, uint32_t fill_color) {
    graphics_rectangle(x, y, width, height, background_color);
    graphics_rectangle(x, y, filled, height, fill_color);
}