#include "syscall.h"
#include "process.h"
#include "thread.h"
#include "ipc.h"
#include "../interrupts/interrupts.h"
#include "../memory/address_space.h"
#include "../memory/vmm.h"
#include "../framebuffer/framebuffer_console.h"
#include "../storage/vfs.h"

static int syscall_copy_user_path(const char* user_path, char path[VFS_PATH_MAX]) {
    if (!user_path) {
        return 0;
    }

    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;
    uint64_t address = (uint64_t)(uintptr_t)user_path;

    for (uint32_t i = 0; i < VFS_PATH_MAX; i++) {
        if (address > UINT64_MAX - i || !address_space_validate_user_buffer(address_space, address + i, 1, 0)) {
            return 0;
        }

        path[i] = user_path[i];

        if (path[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static uint64_t syscall_write(uint64_t file_descriptor, const char* user_buffer, uint64_t length) {
    if (!user_buffer || length > 4096) {
        return SYSCALL_ERROR;
    }

    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;

    if (!address_space_validate_user_buffer(address_space, (uint64_t)user_buffer, length, 0)) {
        return SYSCALL_ERROR;
    }

    if (file_descriptor == 1) {
        char character[2] = {0, '\0'};
        
        for (uint64_t i = 0; i < length; i++) {
            character[0] = user_buffer[i];
            framebuffer_console_print(character);
        }

        return length;
    }

    if (file_descriptor < 3) {
        return SYSCALL_ERROR;
    }

    uint32_t bytes_written = 0;

    if (!process_fd_write(g_current_process, (uint32_t)file_descriptor, user_buffer, (uint32_t)length, &bytes_written)) {
        return SYSCALL_ERROR;
    }

    return bytes_written;
}

static uint64_t syscall_ipc_send(uint32_t receiver_pid, const void* user_data, uint32_t size) {
    if (size == 0 || size > IPC_MESSAGE_DATA_SIZE) {
        return IPC_INVALID;
    }

    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;

    if (!address_space_validate_user_buffer(address_space, (uint64_t)(uintptr_t)user_data, size, 0)) {
        return IPC_INVALID;
    }

    return ipc_send(receiver_pid, user_data, size);
}

static uint64_t syscall_ipc_receive(uint32_t receiver_pid, IpcMessage* user_message) {
    IpcMessage message;
    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;

    if (!address_space_validate_user_buffer(address_space, (uint64_t)(uintptr_t)user_message, sizeof(IpcMessage), VMM_WRITABLE)) {
        return IPC_INVALID;
    }

    IpcResult result = ipc_receive(receiver_pid, &message);

    if (result != IPC_OK) {
        return result;
    }

    for (uint32_t i = 0; i < sizeof(IpcMessage); i++) {
        ((uint8_t*)user_message)[i] = ((uint8_t*)&message)[i];
    }

    return IPC_OK;
}

static uint64_t syscall_open(const char* user_path) {
    char path[VFS_PATH_MAX];

    if (!syscall_copy_user_path(user_path, path)) {
        return SYSCALL_ERROR;
    }

    int file_descriptor = process_fd_open(g_current_process, path);
    return file_descriptor < 3 ? SYSCALL_ERROR : (uint64_t)file_descriptor;
}

static uint64_t syscall_read(uint64_t file_descriptor, void* user_buffer, uint64_t length) {
    if (!user_buffer || length > 4096) {
        return SYSCALL_ERROR;
    }

    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;

    if (!address_space_validate_user_buffer(address_space, (uint64_t)(uintptr_t)user_buffer, length, VMM_WRITABLE)) {
        return SYSCALL_ERROR;
    }

    uint32_t bytes_read = 0;

    if (!process_fd_read(g_current_process, (uint32_t)file_descriptor, user_buffer, (uint32_t)length, &bytes_read)) {
        return SYSCALL_ERROR;
    }

    return bytes_read;
}

static uint64_t syscall_seek(uint64_t file_descriptor, int64_t offset, uint64_t whence) {
    if (whence > FILESYSTEM_SEEK_END || !process_fd_seek(g_current_process, (uint32_t)file_descriptor, offset, (FilesystemSeekWhence)whence)) {
        return SYSCALL_ERROR;
    }

    return 1;
}

static uint64_t syscall_close(uint64_t file_descriptor) {
    return process_fd_close(g_current_process, (uint32_t)file_descriptor) ? 1 : SYSCALL_ERROR;
}

uint64_t syscall_dispatch(struct InterruptFrame* frame) {
    if (!frame || !g_current_process) {
        return UINT64_MAX;
    }

    switch (frame->rax) {
        case SYSCALL_GETPID:
            return g_current_process->pid;

        case SYSCALL_YIELD:
            thread_yield();
            return 0;

        case SYSCALL_EXIT:
            thread_exit();

            while (1) {
                __asm__ volatile ("cli; hlt");
            }

        case SYSCALL_WRITE:
            return syscall_write(frame->rdi, (const char*)(uintptr_t)frame->rsi, frame->rdx);

        case SYSCALL_IPC_SEND:
            return syscall_ipc_send((uint32_t)frame->rdi, (const void*)(uintptr_t)frame->rsi, (uint32_t)frame->rdx);

        case SYSCALL_IPC_RECEIVE:
            return syscall_ipc_receive((uint32_t)frame->rdi, (IpcMessage*)(uintptr_t)frame->rsi);

        case SYSCALL_OPEN:
            return syscall_open((const char*)(uintptr_t)frame->rdi);

        case SYSCALL_READ:
            return syscall_read(frame->rdi, (void*)(uintptr_t)frame->rsi, frame->rdx);

        case SYSCALL_SEEK:
            return syscall_seek(frame->rdi, (int64_t)frame->rsi, frame->rdx);

        case SYSCALL_CLOSE:
            return syscall_close(frame->rdi);

        default:
            return UINT64_MAX;
    }
}