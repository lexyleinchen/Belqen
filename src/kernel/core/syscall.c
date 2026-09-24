#include "syscall.h"
#include "process.h"
#include "thread.h"
#include "ipc.h"
#include "../interrupts/interrupts.h"
#include "../memory/address_space.h"
#include "../memory/vmm.h"
#include "../drivers/serial/serial.h"

static uint64_t syscall_write(uint64_t file_descriptor, const char* user_buffer, uint64_t length) {
    if (file_descriptor != 1 || !user_buffer) {
        return SYSCALL_ERROR;
    }

    if (length > 4096) {
        return SYSCALL_ERROR;
    }

    AddressSpace* address_space = (AddressSpace*)g_current_process->address_space;

    if (!address_space_validate_user_buffer(address_space, (uint64_t)user_buffer, length, 0)) {
        return SYSCALL_ERROR;
    }

    for (uint64_t i = 0; i < length; i++) {
        serial_write_char(user_buffer[i]);
    }

    return length;
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

        default:
            return UINT64_MAX;
    }
}