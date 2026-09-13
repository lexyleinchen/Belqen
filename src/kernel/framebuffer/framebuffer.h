#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Framebuffer {
    uintptr_t address; 
    uint32_t width; 
    uint32_t height; 
    uint32_t pitch;
} Framebuffer;

void framebuffer_init(Framebuffer fb);

Framebuffer framebuffer_get(void);

#ifdef __cplusplus
}
#endif

#endif // FRAMEBUFFER_H