#ifndef _INT_COM_H_
#define _INT_COM_H_

typedef enum {
    NONE,
    INITIALIZE_HARTS,
    KERNEL_PANIC,
    YIELD_TASK,
    KILL_TASK,
} MessageType;

void sendMachineSoftwareInterrupt(int hart);

void clearMachineSoftwareInterrupt(int hart);

void sendMessageTo(int hartid, MessageType type, void* data);

void sendMessageToAll(MessageType type, void* data);

void sendMessageToSelf(MessageType type, void* data);

void handleMessage(MessageType type, void* data);

void handleMachineSoftwareInterrupt();

#endif
