#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H

#include <stdint.h>
#include "../kernel/core/ipc.h"

#define USER_SYSCALL_GETPID 0
#define USER_SYSCALL_YIELD 1
#define USER_SYSCALL_EXIT 2
#define USER_SYSCALL_WRITE 3
#define USER_SYSCALL_IPC_SEND 4
#define USER_SYSCALL_IPC_RECEIVE 5
#define USER_SYSCALL_OPEN 6
#define USER_SYSCALL_READ 7
#define USER_SYSCALL_SEEK 8
#define USER_SYSCALL_CLOSE 9

static uint64_t user_syscall0(uint64_t number) {
    uint64_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

static uint64_t user_syscall1(uint64_t number, uint64_t arg) {
    uint64_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "D"(arg) : "memory");
    return result;
}

static uint64_t user_syscall2(uint64_t number, uint64_t arg1, uint64_t arg2) {
    uint64_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "D"(arg1), "S"(arg2) : "memory");
    return result;
}

static uint64_t user_syscall3(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3) : "memory");
    return result;
}

static uint64_t user_getpid(void) {
    return user_syscall0(USER_SYSCALL_GETPID);
}

static uint64_t user_write(const void* buffer, uint64_t length) {
    return user_syscall3(USER_SYSCALL_WRITE, 1, (uint64_t)(uintptr_t)buffer, length);
}

static uint64_t user_open(const char* path) {
    return user_syscall1(USER_SYSCALL_OPEN, (uint64_t)(uintptr_t)path);
}

static uint64_t user_read(int file_descriptor, void* buffer, uint32_t size) {
    return user_syscall3(USER_SYSCALL_READ, (uint64_t)file_descriptor, (uint64_t)(uintptr_t)buffer, size);
}

static uint64_t user_write_fd(int file_descriptor, const void* buffer, uint64_t length) {
    return user_syscall3(USER_SYSCALL_WRITE, (uint64_t)file_descriptor, (uint64_t)(uintptr_t)buffer, length);
}

static uint64_t user_seek(int file_descriptor, int64_t offset, uint64_t whence) {
    return user_syscall3(USER_SYSCALL_SEEK, (uint64_t)file_descriptor, (uint64_t)offset, whence);
}

static uint64_t user_close(int file_descriptor) {
    return user_syscall1(USER_SYSCALL_CLOSE, (uint64_t)file_descriptor);
}

static void user_yield(void) {
    user_syscall0(USER_SYSCALL_YIELD);
}

static void user_exit(uint64_t status) {
    user_syscall1(USER_SYSCALL_EXIT, status);

    while (1) {
        __asm__ volatile ("hlt");
    }
}

static int user_ipc_send(uint32_t receiver_pid, const void* data, uint32_t size) {
    return (int)user_syscall3(USER_SYSCALL_IPC_SEND, receiver_pid, (uint64_t)(uintptr_t)data, size);
}

static int user_ipc_receive(uint32_t receiver_pid, IpcMessage* message) {
    return (int)user_syscall2(USER_SYSCALL_IPC_RECEIVE, receiver_pid, (uint64_t)(uintptr_t)message);
}

#endif // OS_SYSCALL_H