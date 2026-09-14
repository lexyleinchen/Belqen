#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);

void serial_write_char(char c);

void serial_write_string(const char* string);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_H