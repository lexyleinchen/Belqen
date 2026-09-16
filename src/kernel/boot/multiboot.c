#include "multiboot.h"
#include "../framebuffer/framebuffer.h"
#include "../core/log.h"

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT_TAG_TYPE_ACPI_OLD 14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW 15

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

static uint8_t multiboot_rsdp_data[36];
static const void* multiboot_rsdp = 0;
static uint32_t multiboot_address;

void multiboot_init(uint32_t address) {
    kernel_log("initializing multiboot...");
    uint32_t current = address + 8;
    multiboot_address = address;

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

        if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD || tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
            const uint8_t* rsdp_data = (const uint8_t*)tag + 8;

            for (uint32_t i = 0; i < 36; i++) {
                multiboot_rsdp_data[i] = rsdp_data[i];
            }

            multiboot_rsdp = multiboot_rsdp_data;
            kernel_log("Acpi tag type %u size %u", tag->type, tag->size);
            kernel_log("Rsdp revision byte %u", (uint32_t)multiboot_rsdp_data[15]);
        }

        current += ((tag->size + 7) & ~7); // Align to 8 bytes
    }

    if (multiboot_rsdp) {
        kernel_log("Acpi rsdp found.");
    }
    else {
        kernel_log("Acpi rsdp not found.");
    }

    kernel_log("multiboot started.");
}

const void* multiboot_get_rsdp(void) {
    return multiboot_rsdp;
}

uint32_t multiboot_get_address(void) {
    return multiboot_address;
}