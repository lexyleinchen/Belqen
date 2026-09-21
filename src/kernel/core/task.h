#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "process.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_MAX_COUNT THREAD_MAX_COUNT
#define TASK_NAME_LENGTH 32
#define TASK_KERNEL_STACK_SIZE THREAD_KERNEL_STACK_SIZE

typedef Thread Task;
typedef ThreadContext TaskContext;

void task_init(void);

Task* task_create(const char* name, void* entry, void* arg, uint32_t priority);

#ifdef __cplusplus
}
#endif

#endif // TASK_H