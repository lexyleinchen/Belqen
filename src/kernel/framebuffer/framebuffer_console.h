#ifndef FRAMEBUFFER_CONSOLE_H
#define FRAMEBUFFER_CONSOLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void framebuffer_console_init(void);

void framebuffer_console_print(const char* text);

#ifdef __cplusplus
}
#endif
#endif // FRAMEBUFFER_CONSOLE_H