#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc.h"

#define IPC_MAGIC_IN    0xae67e45b
#define IPC_MAGIC_OUT   0x64738358

#define IPC_STATUS_CONNECTED    0x01
#define IPC_STATUS_READ_READY   0x02
#define IPC_STATUS_ERROR        0x04

#define IPC_REG_KEY         0x00
#define IPC_REG_STATUS      0x04
#define IPC_REG_RAM_ADDR    0x08
#define IPC_REG_WRITE_LEN   0x0c
#define IPC_REG_READ_LEN    0x10

#define IPC_REG(x) (((x) & 0xffff) >> 2)

static char sIpcSockPath[512];

static void init_ipc_path(void)
{
    char buffer[512];
    const char* rt_dir;

    /* Get the path to the IPC dir & create it */
    rt_dir = getenv("XDG_RUNTIME_DIR");
    if (rt_dir == NULL)
        return;
    
    snprintf(buffer, sizeof(buffer), "%s/n64-ipc", rt_dir);
    mkdir(buffer, 0700);

    /* Get the full socket path */
    snprintf(sIpcSockPath, sizeof(sIpcSockPath), "%s/%d.sock", buffer, getpid());
}

void init_ipc(struct ipc* ipc)
{
    init_ipc_path();
    ipc->sock_listen = -1;
    ipc->sock_client = -1;
}

void poweron_ipc(struct ipc* ipc)
{
    close_ipc(ipc);

    ipc->regs[IPC_REG_KEY] = IPC_MAGIC_OUT;
    ipc->regs[IPC_REG_STATUS] = 0;
    ipc->regs[IPC_REG_RAM_ADDR] = 0;
    ipc->regs[IPC_REG_WRITE_LEN] = 0;
    ipc->regs[IPC_REG_READ_LEN] = 0;
}

void close_ipc(struct ipc* ipc)
{
    ipc->isEnabled = 0;

    /* Close both sockets */
    if (ipc->sock_listen > -1)
    {
        close(ipc->sock_listen);
        ipc->sock_listen = -1;
    }

    if (ipc->sock_client > -1)
    {
        close(ipc->sock_client);
        ipc->sock_client = -1;
    }

    /* Ensure the socket file is deleted */
    unlink(sIpcSockPath);
}

static void open_ipc(struct ipc* ipc)
{
    int sock;

    if (ipc->isEnabled)
        return;
    ipc->isEnabled = 1;

    /* Create the socket */
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        close_ipc(ipc);
        return;
    }

    /* Bind the socket to the path */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sIpcSockPath, sizeof(addr.sun_path) - 1);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        close_ipc(ipc);
        return;
    }

    ipc->sock_listen = sock;
}

void read_ipc_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct ipc* ipc = opaque;
    int index;

    if (!ipc->isEnabled)
    {
        *value = (address & 0xffff);
        *value |= (*value << 16);
        return;
    }

    index = IPC_REG(address);
    if (index >= (sizeof(ipc->regs) / sizeof(ipc->regs[0])))
    {
        *value = 0;
        return;
    }
    *value = ipc->regs[index];
}

void write_ipc_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct ipc* ipc = opaque;
    int reg;

    reg = IPC_REG(address);
    if (!ipc->isEnabled && reg != IPC_REG_KEY)
        return;

    switch (reg)
    {
    case IPC_REG_KEY:
        if (value != IPC_MAGIC_IN)
            close_ipc(ipc);
        else
            open_ipc(ipc);
        break;
    }
}