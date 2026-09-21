#include "ipc.h"
#include "process.h"
#include "thread.h"
#include "scheduler.h"

typedef struct IpcMailbox {
    Process* process;
    uint32_t owner_pid;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    IpcMessage messages[IPC_MAX_MESSAGES];
} IpcMailbox;

static IpcMailbox mailboxes[PROCESS_MAX_COUNT];

static void ipc_clear_mailbox(IpcMailbox* mailbox) {
    mailbox->process = 0;
    mailbox->owner_pid = 0;
    mailbox->head = 0;
    mailbox->tail = 0;
    mailbox->count = 0;
}

static IpcMailbox* ipc_find_mailbox(Process* process) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (mailboxes[i].process == process) {
            if (mailboxes[i].owner_pid != process->pid) {
                ipc_clear_mailbox(&mailboxes[i]);
                return &mailboxes[i];
            }

            return &mailboxes[i];
        }
    }

    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (!mailboxes[i].process) {
            mailboxes[i].process = process;
            mailboxes[i].owner_pid = process->pid;
            return &mailboxes[i];
        }
    }

    return 0;
}

void ipc_init(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        ipc_clear_mailbox(&mailboxes[i]);
    }
}

IpcResult ipc_send(uint32_t receiver_pid, const void* data, uint32_t size) {
    if (!data || !size) {
        return IPC_INVALID;
    }

    if (size > IPC_MESSAGE_DATA_SIZE) {
        return IPC_TOO_LAGRE;
    }

    Process* receiver = process_find_by_pid(receiver_pid);

    if (!receiver || receiver->state == PROCESS_STATE_DEAD || receiver->state == PROCESS_STATE_ZOMBIE) {
        return IPC_INVALID;
    }

    uint64_t flags = irq_save();
    IpcMailbox* mailbox = ipc_find_mailbox(receiver);

    if (!mailbox) {
        irq_restore(flags);
        return IPC_ERROR;
    }

    if (mailbox->count >= IPC_MAX_MESSAGES) {
        irq_restore(flags);
        return IPC_FULL;
    }

    IpcMessage* message = &mailbox->messages[mailbox->tail];
    message->sender_pid = g_current_process ? g_current_process->pid : 0;
    message->sender_tid = g_current_thread ? g_current_thread->tid : 0;
    message->size = size;

    for (uint32_t i = 0; i < size; i++) {
        message->data[i] = ((const uint8_t*)data)[i];
    }

    mailbox->tail = (mailbox->tail + 1) % IPC_MAX_MESSAGES;
    mailbox->count++;
    irq_restore(flags);
    return IPC_OK;
}

IpcResult ipc_receive(uint32_t receiver_pid, IpcMessage* message) {
    if (!message) {
        return IPC_INVALID;
    }

    Process* receiver = process_find_by_pid(receiver_pid);

    if (!receiver || receiver->state == PROCESS_STATE_DEAD || receiver->state == PROCESS_STATE_ZOMBIE) {
        return IPC_INVALID;
    }

    uint64_t flags = irq_save();
    IpcMailbox* mailbox = ipc_find_mailbox(receiver);

    if (!mailbox) {
        irq_restore(flags);
        return IPC_ERROR;
    }

    if (mailbox->count == 0) {
        irq_restore(flags);
        return IPC_EMPTY;
    }

    IpcMessage* queued_message = &mailbox->messages[mailbox->head];
    message->sender_pid = queued_message->sender_pid;
    message->sender_tid = queued_message->sender_tid;
    message->size = queued_message->size;

    for (uint32_t i = 0; i < queued_message->size; i++) {
        message->data[i] = queued_message->data[i];
    }

    mailbox->head = (mailbox->head + 1) % IPC_MAX_MESSAGES;
    mailbox->count--;
    irq_restore(flags);
    return IPC_OK;
}

IpcResult ipc_receive_wait(uint32_t receiver_pid, IpcMessage* message) {
    IpcResult result;

    while (1) {
        result = ipc_receive(receiver_pid, message);

        if (result != IPC_EMPTY) {
            return result;
        }

        thread_yield();
    }
}