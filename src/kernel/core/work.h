#ifndef WORK_H
#define WORK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_KERNEL_WORK 32

typedef struct KernelWork KernelWork;

typedef int (*KernelWorkStep)(KernelWork* work);

struct KernelWork {
    KernelWorkStep step;
    void* data;
    uint8_t active;
    uint8_t finished;
    uint32_t id;
    uint64_t wake_tick;
};

typedef struct {
    uint32_t progress;
} TestWork;

void work_init(void);

int work_submit(KernelWorkStep step, void* data);

int work_submit_delayed(KernelWorkStep step, void* data, uint64_t delay_ms);

void work_update(void);

int work_is_finished(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif // WORK_H