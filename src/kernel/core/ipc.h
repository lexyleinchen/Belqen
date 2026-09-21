#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_MAX_MESSAGES 16
#define IPC_MESSAGE_DATA_SIZE 128

typedef enum {
    IPC_OK = 0,
    IPC_ERROR = -1,
    IPC_EMPTY = -2,
    IPC_FULL = -3,
    IPC_INVALID = -4,
    IPC_TOO_LAGRE = -5
} IpcResult;

typedef struct IpcMessage {
    uint32_t sender_pid;
    uint32_t sender_tid;
    uint32_t size;
    uint8_t data[IPC_MESSAGE_DATA_SIZE];
} IpcMessage;

void ipc_init(void);

IpcResult ipc_send(uint32_t receiver_pid, const void* data, uint32_t size);

IpcResult ipc_receive(uint32_t receiver_pid, IpcMessage* message);

IpcResult ipc_receive_wait(uint32_t receiver_pid, IpcMessage* message);

#ifdef __cplusplus
}
#endif

#endif // IPC_H