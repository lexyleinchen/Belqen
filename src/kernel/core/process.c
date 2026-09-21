#include "process.h"

static Process processes[PROCESS_MAX_COUNT];
static Process* process_head = 0;
static Process* process_tail = 0;
static uint32_t next_pid = 1;
Process* g_current_process;

static void process_reap(Process* process) {
    for (uint32_t i = 0; i < process->child_count; i++) {
        if (process->children[i]) {
            process->children[i]->parent = 0;
            process->children[i]->parent_pid = 0;
        }
    }

    Process* parent = process->parent;

    if (parent) {
        for (uint32_t i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == process) {
                parent->children[i] = parent->children[parent->child_count--];
                parent->children[parent->child_count] = 0;
                break;
            } 
        }
    }

    if (process->previous) {
        process->previous->next = process->next;
    }
    else {
        process_head = process->next;
    }

    if (process->next) {
        process->next->previous = process->previous;
    }
    else {
        process_tail = process->previous;
    }

    process->pid = 0;
    process->parent_pid = 0;
    process->parent = 0;
    process->child_count = 0;
    process->thread_count = 0;
    process->name[0] = 0;
    process->state = PROCESS_STATE_DEAD;
    process->next = 0;
    process->previous = 0;
}

static Process* process_find_free_slot(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (processes[i].state == PROCESS_STATE_ZOMBIE && &processes[i] != g_current_process) {
            process_reap(&processes[i]);
        }

        if (processes[i].pid == 0) {
            return &processes[i];
        }
    }

    return 0;
}

static void process_enqueue(Process* process) {
    if (!process) {
        return;
    }

    process->next = 0;
    process->previous = 0;

    if (process_head == 0) {
        process_head = process;
        process_tail = process;
        return;
    }

    process_tail->next = process;
    process->previous = process_tail;
    process_tail = process;
}

void process_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        processes[i].pid = 0;
        processes[i].parent_pid = 0;
        processes[i].name[0] = 0;
        processes[i].state = PROCESS_STATE_DEAD;
        processes[i].thread_count = 0;

        for (uint32_t j = 0; j < 8; j++) {
            processes[i].threads[j] = 0;
            processes[i].children[j] = 0;
        }

        processes[i].parent = 0;
        processes[i].child_count = 0;
        processes[i].address_space = 0;
        processes[i].exit_code = 0;
        processes[i].flags = 0;
        processes[i].next = 0;
        processes[i].previous = 0;
    }

    process_head = 0;
    process_tail = 0;
    next_pid = 1;
    g_current_process = 0;
}

Process* process_create(const char* name) {
    Process* process = process_find_free_slot();

    if (!process) {
        return 0;
    }

    process->pid = next_pid++;
    process->parent_pid = g_current_process ? g_current_process->pid : 0;
    process->parent = g_current_process;
    process->state = PROCESS_STATE_NEW;
    process->thread_count = 0;
    process->child_count = 0;
    process->address_space = 0;
    process->exit_code = 0;
    process->flags = 0;
    process->next = 0;
    process->previous = 0;

    for (uint32_t j = 0; j < 8; j++) {
        process->threads[j] = 0;
        process->children[j] = 0;
    }

    for (uint32_t i = 0; i < PROCESS_NAME_LENGTH - 1; i++) {
        process->name[i] = name ? name[i] : 0;

        if (name && name[i] == 0) {
            break;
        }
    }

    process->name[PROCESS_NAME_LENGTH - 1] = 0;

    if (process->parent && process->parent->child_count < 8) {
        process->parent->children[process->parent->child_count++] = process;
    }

    process_enqueue(process);
    return process;
}

Process* process_find_by_pid(uint32_t pid) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }

    return 0;
}

void process_set_state(Process* process, ProcessState state) {
    if (!process) {
        return;
    }

    process->state = state;
}

void process_exit(Process* process, uint32_t exit_code) {
    if (!process) {
        return;
    }

    process->state = PROCESS_STATE_ZOMBIE;
    process->exit_code = exit_code;
}