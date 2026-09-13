#include "diskmanager.h"

#include "../../os/app_registry.h"
#include "../../os/font.h"
#include "../../os/ui.h"
#include "../../os/graphics.h"

#include "../../kernel/storage/storage.h"
#include "../../kernel/storage/block.h"
#include "../../kernel/storage/partition/partition.h"
#include "../../kernel/storage/partition/filesystem/fat32/fat32.h"
#include "../../kernel/storage/partition/filesystem/filesystem.h"
#include "../../kernel/core/log.h"

namespace diskmanager {
    static Window window;
    static BlockDevice* selected_disk = nullptr;
    static int selected_partition = -1;
    static FAT32FormatWork format_work;
    static BlockDevice* formatting_partition = nullptr;
    static bool formatting = false;

    enum Screen {
        SCREEN_MAIN,
        SCREEN_CREATE_PARTITION,
        SCREEN_FORMATTING,
        SCREEN_CONFIRM
    };

    enum ConfirmAction {
        CONFIRM_CREATE_MBR,
        CONFIRM_DELETE_PARTITION
    };

    static Screen screen = SCREEN_MAIN;
    static ConfirmAction confirm_action;
    static uint32_t create_size_mib = 8192;

    static uint32_t sectors_to_mib(uint64_t sectors, uint64_t sector_size) {
        uint64_t bytes = sectors * sector_size;
        return bytes / (1024 * 1024);
    }

    static void draw_disk_info(int x, int y, BlockDevice* disk) {
        font_draw_text(x, y, "Disk 0", 0xFFFFFFFF);
        font_draw_text(x, y + 22, "Sectors:", 0xFFFFFFFF);
        font_draw_number(x + 120, y + 22, (uint32_t)disk->sector_count, 0xFFFFFFFF);
        font_draw_text(x, y + 44, "Size MiB:", 0xFFFFFFFF);
        uint32_t size_mib = sectors_to_mib((uint32_t)disk->sector_count, disk->sector_size);
        font_draw_number(x + 135, y + 44, (uint32_t)size_mib, 0xFFFFFFFF);
    }

    static void draw_partition(int x, int y, uint32_t number, MBRPartitionInfo* partition) {
        font_draw_text(x, y, "Partition", 0xFFFFFFFF);
        font_draw_number(x + 135, y, number + 1, 0xFFFFFFFF);
        font_draw_text(x + 190, y, "Type:", 0xFFAAAAAA);
        font_draw_number(x + 260, y, partition->type, 0xFFFFFFFF);
        font_draw_text(x + 330, y, "Size MiB:", 0xFFAAAAAA);
        uint32_t size_mib = sectors_to_mib(partition->sector_count, selected_disk->sector_size);
        font_draw_number(x + 470, y, size_mib, 0xFFFFFFFF);
    }

    static void draw_main(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);
        int content_width = ui_window_content_width(window);
        int content_height = ui_window_content_height(window);
        uint32_t device_count = block_get_device_count();
        int physical_count = 0;

        for (uint32_t i = 0; i < device_count; i++) {
            BlockDevice* device = block_get_device(i);

            if (!device) {
                continue;
            }

            if (device->type == BLOCK_DEVICE_PHYSICAL) {
                physical_count++;

                if (selected_disk == nullptr) {
                    selected_disk = device;
                }
            }
        }

        if (physical_count == 0) {
            font_draw_text(content_x + 20, content_y + 60, "No physical disk found.", 0xFFFFFFFF);
            return;
        }

        draw_disk_info(content_x + 20, content_y + 55, selected_disk);

        if (partition_has_mbr(selected_disk)) {
            font_draw_text(content_x + 300, content_y + 55, "Partition table: MBR", 0xFFFFFFFF);

            for (uint32_t i = 0; i < 4; i++) {
                MBRPartitionInfo partition;

                if (!partition_get_mbr_partition(selected_disk, i, &partition)) {
                    continue;
                }

                if (partition.type == 0 || partition.sector_count == 0) {
                    continue;
                }

                if (selected_partition == (int)i) {
                    graphics_rectangle(content_x + 15, content_y + 115 + i * 45, content_width - 160, 30, 0xFF304050);
                }

                draw_partition(content_x + 20, content_y + 120 + i * 45, i, &partition);

                if (graphics_button(content_x + (content_width - 20 - 100), content_y + 115 + i * 45, 100, 30, 0xFF606060, "Select", 0xFFFFFFFF)) {
                    selected_partition = i;
                }
            }

            if (graphics_button(content_x + 20, content_y + (content_height - 20 - 35), 150, 35, 0xFF606060, "Create", 0xFFFFFFFF)) {
                screen = SCREEN_CREATE_PARTITION;
            }

            if (graphics_button(content_x + 185, content_y + (content_height - 20 - 35), 150, 35, 0xFF606060, "Delete", 0xFFFFFFFF)) {
                if (selected_partition >= 0) {
                    confirm_action = CONFIRM_DELETE_PARTITION;
                    screen = SCREEN_CONFIRM;
                }
            }
        }
        else {
            font_draw_text(content_x + 300, content_y + 55, "Partition table: None", 0xFFFFFFFF);
            font_draw_text(content_x + 20, content_y + 125, "This disk has no MBR.", 0xFFFFFFFF);

            if (graphics_button(content_x + 20, content_y + 170, 180, 35, 0xFF606060, "Create MBR", 0xFFFFFFFF)) {
                confirm_action = CONFIRM_CREATE_MBR;
                screen = SCREEN_CONFIRM;
            }
        }
    }

    static void draw_create_partition(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);
        font_draw_text(content_x + 20, content_y + 20, "Create Partition", 0xFFFFFFFF);
        font_draw_text(content_x + 20, content_y + 70, "Size:", 0xFFFFFFFF);
        font_draw_number(content_x + 100, content_y + 70, create_size_mib, 0xFFFFFFFF);
        font_draw_text(content_x + 160, content_y + 70, "MiB", 0xFFFFFFFF);

        if (graphics_button(content_x + 20, content_y + 110, 80, 35, 0xFF606060, "-", 0xFFFFFFFF)) {
            if (create_size_mib > 1024) {
                create_size_mib -= 1024;
            }
        }

        if (graphics_button(content_x + 110, content_y + 110, 80, 35, 0xFF606060, "+", 0xFFFFFFFF)) {
            create_size_mib += 1024;
        }

        font_draw_text(content_x + 20, content_y + 175, "Partition type:", 0xFFFFFFFF);
        font_draw_text(content_x + 20, content_y + 205, "0x0C FAT32 LBA", 0xFFFFFFFF);

        if (graphics_button(content_x + 20, content_y + 260, 140, 35, 0xFF606060, "Create", 0xFFFFFFFF)) {
            if (selected_disk != nullptr) {
                uint32_t created_partition;

                if (partition_create_mbr_partition_size(selected_disk, create_size_mib, 0x0C, &created_partition)) {
                    partition_scan(selected_disk);
                    BlockDevice* partition = partition_get_device(selected_disk, created_partition);

                    if (!partition) {
                        kernel_log("failed to get created partition.");
                        return;
                    }

                    formatting_partition = partition;

                    if (!fat32_format_async(partition, &format_work)) {
                        kernel_log("failed to format partition as fat32.");
                        formatting_partition = nullptr;
                        formatting = false;
                        return;
                    }

                    formatting = true;
                    screen = SCREEN_FORMATTING;
                    kernel_log("fat32 formatting stared.");
                }
            }
        }

        if (graphics_button(content_x + 185, content_y + 260, 140, 35, 0xFF606060, "Cancel", 0xFFFFFFFF)) {
            screen = SCREEN_MAIN;
        }
    }

    static void draw_formatting(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);
        font_draw_text(content_x + 20, content_y + 20, "Formatting partition", 0xFFFFFFFF);
        uint32_t percent = fat32_format_get_percent(&format_work);
        font_draw_text(content_x + 20, content_y + 80, "Progress:", 0xFFFFFFFF);
        font_draw_number(content_x + 145, content_y + 80, percent, 0xFFFFFFFF);
        font_draw_text(content_x + 180, content_y + 80, "%", 0xFFFFFFFF);
        font_draw_progressbar(content_x + 20, content_y + 125, 500, 25, (500 * percent) / 100, 0xFF404040, 0xFF408040);
        font_draw_text(content_x + 20, content_y + 175, "Please wait...", 0xFFAAAAAA);
    }

    static void draw_confirm(Window* window) {
        int content_x = ui_window_content_x(window);
        int content_y = ui_window_content_y(window);

        if (confirm_action == CONFIRM_CREATE_MBR) {
            font_draw_text(content_x + 20, content_y + 20, "Create new MBR", 0xFFFFFFFF);
            font_draw_text(content_x + 20, content_y + 60, "This will replace the", 0xFFFFFFFF);
            font_draw_text(content_x + 20, content_y + 82, "current partition table.", 0xFFFFFFFF);
        }
        else if (confirm_action == CONFIRM_DELETE_PARTITION) {
            font_draw_text(content_x + 20, content_y + 20, "Delete partition", 0xFFFFFFFF);
            font_draw_text(content_x + 20, content_y + 60, "This partition entry will", 0xFFFFFFFF);
            font_draw_text(content_x + 20, content_y + 82, "be removed from MBR.", 0xFFFFFFFF);       
        }

        if (graphics_button(content_x + 20, content_y + 140, 140, 35, 0xFF606060, "Continue", 0xFFFFFFFF)) {
            if (selected_disk != nullptr) {
                if (confirm_action == CONFIRM_CREATE_MBR) {
                    partition_create_mbr(selected_disk);
                    selected_partition = -1;
                    partition_scan(selected_disk);
                }
                else if (confirm_action == CONFIRM_DELETE_PARTITION) {
                    if (selected_partition >= 0) {
                        BlockDevice* partition = partition_get_device(selected_disk, (uint32_t)selected_partition);

                        if (partition) {
                            storage_unmount_partition(partition);
                        }

                        partition_delete(selected_disk, (uint32_t)selected_partition);
                        selected_partition = -1;
                        partition_scan(selected_disk);
                    }
                }

                screen = SCREEN_MAIN;
            }
        }

        if (graphics_button(content_x + 185, content_y + 140, 140, 35, 0xFF606060, "Cancel", 0xFFFFFFFF)) {
            screen = SCREEN_MAIN;
        }
    }

    static void draw_content(Window* window) {
        if (screen == SCREEN_MAIN) {
            draw_main(window);
        }
        else if (screen == SCREEN_CREATE_PARTITION) {
            draw_create_partition(window);
        }
        else if (screen == SCREEN_FORMATTING) {
            draw_formatting(window);
        }
        else if (screen == SCREEN_CONFIRM) {
            draw_confirm(window);
        }
    }

    static void update_content(Window* window) {
        (void)window;

        if (!formatting) {
            return;
        }

        FAT32FormatStatus status = fat32_format_get_status(&format_work);

        if (status == FAT32_FORMAT_RUNNING) {
            return;
        }

        if (status == FAT32_FORMAT_FAILED) {
            kernel_log("fat32 formatting failed.");
            formatting = false;
            formatting_partition = nullptr;
            screen = SCREEN_MAIN;
            return;
        }

        if (status == FAT32_FORMAT_COMPLETE) {
            kernel_log("fat32 formatting complete.");

            if (!formatting_partition) {
                kernel_log("formatting partition is null.");
                formatting = false;
                screen = SCREEN_MAIN;
                return;
            }

            if (!storage_mount_partition(formatting_partition)) {
                kernel_log("failed to add filesystem to storage.");
                formatting = false;
                formatting_partition = nullptr;
                screen = SCREEN_MAIN;
                return;
            }

            kernel_log("filesystem added to storage.");
            formatting = false;
            formatting_partition = nullptr;
            selected_partition = -1;
            screen = SCREEN_MAIN;
        }
    } 

    Window* init() {
        Window* window = ui_create_window(60, 60, 800, 450, "Diskmanager", 0xFF000000, 0xFF808080, 0xFFFFFFFF, update_content, draw_content);
        return window;
    }
}

REGISTER_APP(diskmanager);