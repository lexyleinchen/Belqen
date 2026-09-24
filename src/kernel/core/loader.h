#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include "process.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

int process_load_bpe(const void* image, uint64_t image_size, const char* name, Process** process_result, Thread** thread_result);

#ifdef __cplusplus
}
#endif

#endif // LOADER_H