#ifndef M64P_DEVICE_IPC_H
#define M64P_DEVICE_IPC_H

/* This implements an IPC interface (pseudo serial bus), similar to the USB
 * interfaces present on some flashcarts. */
struct ipc {
    char* sock_path;
    int sock_listen;
    int sock_client;
};

void init_ipc(struct ipc* ipc);
void poweron_ipc(struct ipc* ipc);
void close_ipc(struct ipc* ipc);

#endif
