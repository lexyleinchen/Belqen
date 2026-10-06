#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "../storage/partition/filesystem/filesystem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROCESS_MAX_COUNT 64
#define PROCESS_NAME_LENGTH 32
#define PROCESS_FLAG_USER 0x00000001
#define PROCESS_MAX_FILE_DESCRIPTORS 32

struct Thread;

typedef enum {
    PROCESS_STATE_NEW = 0,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_ZOMBIE,
    PROCESS_STATE_DEAD
} ProcessState;

typedef struct {
    uint8_t used;
    FilesystemFile file;
} ProcessFileDescriptor;

typedef struct Process {
    uint32_t pid;
    uint32_t parent_pid;
    char name[PROCESS_NAME_LENGTH];
    ProcessState state;
    uint32_t thread_count;
    struct Thread* threads[8];
    struct Process* parent;
    struct Process* children[8];
    uint32_t child_count;
    void* address_space;
    uint32_t exit_code;
    uint32_t flags;
    struct Process* next;
    struct Process* previous;
    ProcessFileDescriptor file_descriptors[PROCESS_MAX_FILE_DESCRIPTORS];
} Process;

void process_init(void);

Process* process_create(const char* name);

Process* process_find_by_pid(uint32_t pid);

void process_set_state(Process* process, ProcessState state);

void process_close_descriptors(Process* process);

void process_exit(Process* process, uint32_t exit_code);

Process* process_create_user(const char* name);

int process_fd_open(Process* process, const char* path);

int process_fd_close(Process* process, uint32_t file_descriptor);

int process_fd_read(Process* process, uint32_t file_descriptor, void* buffer, uint32_t size, uint32_t* bytes_read);

int process_fd_write(Process* process, uint32_t file_descriptor, const void* buffer, uint32_t size, uint32_t* bytes_written);

int process_fd_seek(Process* process, uint32_t file_descriptor, int64_t offset, FilesystemSeekWhence whence);

extern Process* g_current_process;

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H