#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_init(void);

void kernel_log(const char* text, ...);

void log_init_console(void);

void log_disable_console(void);

int log_count(void);

const char* log_get_line(int index);

void log_dump_serial(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_H