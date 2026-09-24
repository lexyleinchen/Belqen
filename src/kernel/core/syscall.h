#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct InterruptFrame;

#define SYSCALL_VECTOR 0x80
#define SYSCALL_GETPID 0
#define SYSCALL_YIELD 1
#define SYSCALL_EXIT 2
#define SYSCALL_WRITE 3
#define SYSCALL_IPC_SEND 4
#define SYSCALL_IPC_RECEIVE 5
#define SYSCALL_SUCCESS 0
#define SYSCALL_ERROR UINT64_MAX

uint64_t syscall_dispatch(struct InterruptFrame* frame);

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H