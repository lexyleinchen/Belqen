#include "process.h"
#include "../memory/address_space.h"
#include "../storage/vfs.h"

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
                parent->children[i] = parent->children[--parent->child_count];
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

    if (process->address_space) {
        address_space_destroy((AddressSpace*)process->address_space);
        process->address_space = 0;
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

        for (uint32_t fd = 0; fd < PROCESS_MAX_FILE_DESCRIPTORS; fd++) {
            processes[i].file_descriptors[fd].used = 0;
        }
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

    for (uint32_t fd = 0; fd < PROCESS_MAX_FILE_DESCRIPTORS; fd++) {
        process->file_descriptors[fd].used = 0;
    }

    process_enqueue(process);
    return process;
}

Process* process_create_user(const char* name) {
    Process* process = process_create(name);

    if (!process) {
        return 0;
    }

    AddressSpace* address_space = address_space_create();

    if (!address_space) {
        process_exit(process, 1);
        return 0;
    }

    process->address_space = address_space;
    process->flags |= PROCESS_FLAG_USER;
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

void process_close_descriptors(Process* process) {
    if (!process) {
        return;
    }

    for (uint32_t fd = 3; fd < PROCESS_MAX_FILE_DESCRIPTORS; fd++) {
        ProcessFileDescriptor* descriptor = &process->file_descriptors[fd];

        if (descriptor->used) {
            vfs_close(&descriptor->file);
            descriptor->used = 0;
        }
    }
}

void process_exit(Process* process, uint32_t exit_code) {
    if (!process) {
        return;
    }

    process_close_descriptors(process);
    process->state = PROCESS_STATE_ZOMBIE;
    process->exit_code = exit_code;
}

int process_fd_open(Process* process, const char* path) {
    if (!process || !path) {
        return -1;
    }

    for (uint32_t fd = 3; fd < PROCESS_MAX_FILE_DESCRIPTORS; fd++) {
        ProcessFileDescriptor* descriptor = &process->file_descriptors[fd];

        if (descriptor->used) {
            continue;
        }

        if (!vfs_open_path(path, &descriptor->file)) {
            return -1;
        }

        descriptor->used = 1;
        return(int)fd;
    }

    return -1;
}

int process_fd_close(Process* process, uint32_t file_descriptor) {
    if (!process || file_descriptor < 3 ||  file_descriptor >= PROCESS_MAX_FILE_DESCRIPTORS || !process->file_descriptors[file_descriptor].used) {
        return 0;
    }

    vfs_close(&process->file_descriptors[file_descriptor].file);
    process->file_descriptors[file_descriptor].used = 0;
    return 1;
}

int process_fd_read(Process* process, uint32_t file_descriptor, void* buffer, uint32_t size, uint32_t* bytes_read) {
    if (!process || file_descriptor < 3 || file_descriptor >= PROCESS_MAX_FILE_DESCRIPTORS || !process->file_descriptors[file_descriptor].used) {
        return 0;
    }

    return vfs_read(&process->file_descriptors[file_descriptor].file, buffer, size, bytes_read);
}

int process_fd_write(Process* process, uint32_t file_descriptor, const void* buffer, uint32_t size, uint32_t* bytes_written) {
    if (!process || file_descriptor < 3 || file_descriptor >= PROCESS_MAX_FILE_DESCRIPTORS || !process->file_descriptors[file_descriptor].used) {
        return 0;
    }

    return vfs_write(&process->file_descriptors[file_descriptor].file, buffer, size, bytes_written);
}

int process_fd_seek(Process* process, uint32_t file_descriptor, int64_t offset, FilesystemSeekWhence whence) {
    if (!process || file_descriptor < 3 || file_descriptor >= PROCESS_MAX_FILE_DESCRIPTORS || !process->file_descriptors[file_descriptor].used) {
        return 0;
    }

    return vfs_seek(&process->file_descriptors[file_descriptor].file, offset, whence);
}