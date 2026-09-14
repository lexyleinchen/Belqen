#include "work.h"
#include "../interrupts/interrupts.h"
#include "log.h"

static KernelWork works[MAX_KERNEL_WORK];
static uint32_t next_work_id = 1;
static TestWork test_work;

static int test_work_step(KernelWork* work) {
    TestWork* data = (TestWork*)work->data;
    data->progress++;
    kernel_log("work progress %u", data->progress);
    
    if (data->progress >= 100000) {
        return 1;
    }

    return 0;
}

void work_init(void) {
    kernel_log("Initializing kernel work scheduler...");

    for (uint32_t i = 0; i < MAX_KERNEL_WORK; i++) {
        works[i].active = 0;
        works[i].finished = 0;
        works[i].step = 0;
        works[i].data = 0;
        works[i].id = 0;
        works[i].wake_tick = 0;
    }

    next_work_id = 1;
    kernel_log("kernel work scheduler initialized.");
}

int work_submit(KernelWorkStep step, void* data) {
    if (!step) {
        return 0;
    }

    for (uint32_t i = 0; i < MAX_KERNEL_WORK; i++) {
        if (!works[i].active) {
            works[i].step = step;
            works[i].data = data;
            works[i].active = 1;
            works[i].finished = 0;
            works[i].id = next_work_id++;
            works[i].wake_tick = timer_get_ticks();

            if (next_work_id == 0) {
                next_work_id = 1;
            }

            return (int)works[i].id;
        }
    }

    kernel_log("work scheduler is full.");
    return 0;
}

int work_submit_delayed(KernelWorkStep step, void* data, uint64_t delay_ms) {
    int work_id = work_submit(step, data);

    if (!work_id) {
        return 0;
    }

    uint64_t delay_ticks = (delay_ms + 9) / 10;
    uint64_t wake_tick = timer_get_ticks() + delay_ticks;

    for (uint32_t i = 0; i < MAX_KERNEL_WORK; i++) {
        if (works[i].id == (uint32_t)work_id) {
            works[i].wake_tick = wake_tick;
            break;
        }
    }

    return work_id;
}

void work_update(void) {
    for (uint32_t i = 0; i < MAX_KERNEL_WORK; i++) {
        if (!works[i].active) {
            continue;
        }

        if (timer_get_ticks() < works[i].wake_tick) {
            continue;
        }

        if (!works[i].step) {
            works[i].active = 0;
            works[i].finished = 1;
            continue;
        }

        int result = works[i].step(&works[i]);

        if (result) {
            works[i].active = 0;
            works[i].finished = 1;
        }
    }
}

int work_is_finished(uint32_t id) {
    if (id == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < MAX_KERNEL_WORK; i++) {
        if (works[i].id == id) {
            return works[i].finished;
        }
    }

    return 0;
}