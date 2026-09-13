#include "multiboot.h"
#include "../framebuffer/framebuffer.h"
#include "../core/log.h"

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

typedef struct MultibootTag {
    uint32_t type;
    uint32_t size;
} MultibootTag;

typedef struct MultibootTagFramebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved;
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
} MultibootTagFramebuffer;

void multiboot_init(uint32_t address) {
    kernel_log("initializing multiboot...");
    uint32_t current = address + 8; // Skip the total size and reserved fields

    while (1) {
        MultibootTag* tag = (MultibootTag*)(uintptr_t)current;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            MultibootTagFramebuffer* framebuffer_tag = (MultibootTagFramebuffer*)tag;
            Framebuffer framebuffer;
            framebuffer.address = (uintptr_t)framebuffer_tag->framebuffer_addr;
            framebuffer.width = framebuffer_tag->framebuffer_width;
            framebuffer.height = framebuffer_tag->framebuffer_height;
            framebuffer.pitch = framebuffer_tag->framebuffer_pitch;

            framebuffer_init(framebuffer);
        }

        current += ((tag->size + 7) & ~7); // Align to 8 bytes
    }
    kernel_log("multiboot started.");
}