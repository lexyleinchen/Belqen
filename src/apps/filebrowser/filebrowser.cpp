#include "filebrowser.h"

#include "../../os/app_registry.h"
#include "../../os/font.h"
#include "../../os/ui.h"

#include "../../kernel/storage/storage.h"
#include "../../kernel/storage/partition/filesystem/filesystem.h"
#include "../../kernel/core/log.h"

namespace filebrowser {
    static Filesystem* current_filesystem = nullptr;
    static FilesystemEntry entries[64];
    static uint32_t entry_count = 0;
    static uint32_t current_cluster = 0;
    static bool browsing = false;
    static uint32_t parent_stack[32];
    static uint32_t parent_stack_count = 0;
    static const char* new_file_name = "NEWFILE.TXT";
    static const char* new_directory_name = "NEWDIR";

    static void load_directory(uint32_t cluster) {
        if (!current_filesystem) {
            return;
        }

        entry_count = 0;

        if (!filesystem_read_directory(current_filesystem, cluster, entries, 64, &entry_count)) {
            kernel_log("filebrowser failed to read directory.");
            return;
        }

        current_cluster = cluster;
    }

    static void open_filesystem(uint32_t index) {
        Filesystem* filesystem = storage_get_filesystem(index);

        if (!filesystem) {
            return;
        }

        current_filesystem = filesystem;
        current_cluster = 0;
        load_directory(0);
        browsing = true;
    }

    static void open_directory(uint32_t cluster) {
        if (!current_filesystem) {
            return;
        }

        if (cluster < 2) {
            return;
        }

        if (parent_stack_count < 32) {
            parent_stack[parent_stack_count] = current_cluster;
            parent_stack_count++;
        }

        load_directory(cluster);
    }

    static void go_back() {
        if (!browsing) {
            return;
        }

        if (parent_stack_count > 0) {
            parent_stack_count--;
            uint32_t parent = parent_stack[parent_stack_count];
            load_directory(parent);
            return;
        }

        current_filesystem = nullptr;
        current_cluster = 0;
        entry_count = 0;
        browsing = false;
    }

    static void create_file() {
        if (!current_filesystem) {
            return;
        }

        if (!filesystem_create_file(current_filesystem, current_cluster, new_file_name)) {
            kernel_log("filebrowser failed to create file.");
            return;
        }

        load_directory(current_cluster);
    }

    static void create_directory() {
        if (!current_filesystem) {
            return;
        }

        if (!filesystem_create_directory(current_filesystem, current_cluster, new_directory_name)) {
            kernel_log("filebrowser failed to create directory.");
            return;
        }

        load_directory(current_cluster);
    }

    static void open_file(FilesystemEntry* entry) {
        if (!entry) {
            return;
        }

        kernel_log("filebrowser opening file %s", entry->name);

        // Added after creation texteditor
    }

    static void draw_filesystems(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);
        int content_width = ui_window_content_width(window);
        font_draw_text(content_x + 20, content_y + 20, "Filesystems", 0xFFFFFFFF);
        uint32_t count = storage_get_filesystem_count();
        
        if (count == 0) {
            font_draw_text(content_x + 20, content_y + 60, "No supported filesystems found.", 0xFFFFFFFF);
            return;
        }

        for (uint32_t i = 0; i < count; i++) {
            Filesystem* filesystem = storage_get_filesystem(i);

            if (!filesystem) {
                continue;
            }

            int y = content_y + 60 + (int)i * 45;
            font_draw_text(content_x + 25, y, "Filesystem", 0xFFFFFFFF);
            font_draw_number(content_x + 170, y, i, 0xFFFFFFFF);

            if (filesystem->type == FILESYSTEM_FAT32) {
                font_draw_text(content_x + (content_width / 2), y, "FAT32", 0xFFFFFFFF);
            }
            else {
                font_draw_text(content_x + (content_width / 2), y, "Unknown", 0xFFFFFFFF);
            }

            if (graphics_button(content_x + (content_width - 20 - 100), y - 10, 100, 30, 0xFF606060, "Select", 0xFFFFFFFF)) {
                open_filesystem(i);
            }
        }
    }

    static void draw_directory(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);
        int content_width = ui_window_content_width(window);
        font_draw_text(content_x + 20, content_y + 20, "Directory", 0xFFFFFFFF);

        if (graphics_button(content_x + 20, content_y + 50, 80, 30, 0xFF606060, "Back", 0xFFFFFFFF)) {
            go_back();
            return;
        }

        if (graphics_button(content_x + 115, content_y + 50, 135, 30, 0xFF606060, "New File", 0xFFFFFFFF)) {
            create_file();
        }

        if (graphics_button(content_x + 265, content_y + 50, 160, 30, 0xFF606060, "New Folder", 0xFFFFFFFF)) {
            create_directory();
        }

        for (uint32_t i = 0; i < entry_count; i++) {
            FilesystemEntry* entry = &entries[i];
            int y = content_y + 105 + (int)i * 35;

            if (entry->is_directory) {
                font_draw_text(content_x + 25, y, "[DIR]", 0xFFFFFFFF);
            }
            else {
                font_draw_text(content_x + 25, y, "[FILE]", 0xFFFFFFFF);
            }

            font_draw_text(content_x + 120, y, entry->name, 0xFFFFFFFF);

            if (!entry->is_directory) {
                font_draw_number(content_x + 500, y, entry->size, 0xFFFFFFFF);
                font_draw_text(content_x + 590, y, "bytes", 0xFFFFFFFF);
            }

            if (entry->is_directory) {
                if (graphics_button(content_x + (content_width - 20 - 80), y - 10, 80, 30, 0xFF606060, "Open", 0xFFFFFFFF)) {
                    open_directory(entry->cluster);
                    return;
                }
            }
            else {
                if (graphics_button(content_x + (content_width - 20 - 80), y - 10, 80, 30, 0xFF606060, "Open", 0xFFFFFFFF)) {
                    open_file(entry);
                }
            }
        }
    }

    static void draw_content(Window* window) {
        if (!browsing) {
            draw_filesystems(window);
        }
        else {
            draw_directory(window);
        }
    }

    Window* init() {
        Window* window = ui_create_window(60, 60, 800, 450, "Filebrowser", 0xFF000000, 0xFF808080, 0xFFFFFFFF, nullptr, draw_content);
        return window;
    }
}

REGISTER_APP(filebrowser);