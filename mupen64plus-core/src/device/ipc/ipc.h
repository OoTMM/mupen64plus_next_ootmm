#ifndef M64P_DEVICE_IPC_H
#define M64P_DEVICE_IPC_H

#include <stdint.h>

/* This implements an IPC interface (pseudo serial bus), similar to the USB
 * interfaces present on some flashcarts. */
struct ipc {
    char* sock_path;
    int sock_listen;
    int sock_client;
    unsigned char isEnabled:1;
    uint32_t regs[5];
};

void init_ipc(struct ipc* ipc);
void poweron_ipc(struct ipc* ipc);
void close_ipc(struct ipc* ipc);

void read_ipc_regs(void* opaque, uint32_t address, uint32_t* value);
void write_ipc_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask);

#endif
