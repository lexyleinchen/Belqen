#include "task.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

void task_init(void) {
    process_init();
    thread_init();
}

Task* task_create(const char* name, void* entry, void* arg, uint32_t priority) {
    Process* process = process_create(name);

    if (!process) {
        return 0;
    }

    return thread_create(process, name, entry, arg, priority);
}