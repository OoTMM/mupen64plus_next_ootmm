#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>
#include "ipc.h"
#include "device/device.h"
#include "osal/preproc.h"

#define IPC_MAGIC_IN    0xae67e45b
#define IPC_MAGIC_OUT   0x64738358

#define IPC_STATUS_CONNECTED    0x01
#define IPC_STATUS_READ_READY   0x02
#define IPC_STATUS_ERROR        0x04

#define IPC_REG_KEY         0
#define IPC_REG_STATUS      1
#define IPC_REG_RAM_ADDR    2
#define IPC_REG_WRITE_LEN   3
#define IPC_REG_READ_LEN    4

#define IPC_REG(x) (((x) & 0xffff) >> 2)

static char sIpcSockPath[512];

extern struct device g_dev;

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

    /* Reset registers */
    ipc->regs[IPC_REG_KEY] = IPC_MAGIC_OUT;
    ipc->regs[IPC_REG_STATUS] = 0;
    ipc->regs[IPC_REG_RAM_ADDR] = 0;
    ipc->regs[IPC_REG_WRITE_LEN] = 0;
    ipc->regs[IPC_REG_READ_LEN] = 0;
}

static void ipc_accept_check(struct ipc* ipc)
{
    struct pollfd pfd;
    int sock;

    if (ipc->sock_listen < 0)
        return;
    
    pfd.fd = ipc->sock_listen;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 0) > 0)
    {
        printf("ipc_accept_check: Accepting new client connection\n");

        /* Accept the socket */
        sock = accept(ipc->sock_listen, NULL, NULL);
        if (sock < 0)
            return;
        
        /* If we already have a client, just close the new one */
        if (ipc->sock_client > -1)
        {
            close(sock);
        }
        else
        {
            ipc->sock_client = sock;
            ipc->regs[IPC_REG_STATUS] |= IPC_STATUS_CONNECTED;
        }
    }
}

static void ipc_update_avail(struct ipc* ipc)
{
    if (ipc->sock_client < 0)
        return;

    struct pollfd pfd;
    pfd.fd = ipc->sock_client;
    pfd.events = POLLIN | POLLOUT | POLLERR;
    if (poll(&pfd, 1, 0) > 0)
    {
        if (pfd.revents & POLLIN)
            ipc->regs[IPC_REG_STATUS] |= IPC_STATUS_READ_READY;
        else
            ipc->regs[IPC_REG_STATUS] &= ~IPC_STATUS_READ_READY;
    }
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
        printf("open_ipc: Failed to create socket\n");
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
        printf("open_ipc: Failed to bind socket\n");
        close(sock);
        close_ipc(ipc);
        return;
    }
    
    if (listen(sock, 1) < 0)
    {
        printf("open_ipc: Failed to listen on socket\n");
        close(sock);
        close_ipc(ipc);
        return;
    }

    /* Enable non-blocking mode */
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

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

    ipc_accept_check(ipc);
    ipc_update_avail(ipc);

    index = IPC_REG(address);
    if (index >= (sizeof(ipc->regs) / sizeof(ipc->regs[0])))
    {
        *value = 0;
        return;
    }
    *value = ipc->regs[index];
}

static void ipc_error(struct ipc* ipc)
{
    printf("ipc_error: Client socket error, closing connection\n");
    close(ipc->sock_client);
    ipc->sock_client = -1;
    ipc->regs[IPC_REG_STATUS] = 0;
    ipc->regs[IPC_REG_WRITE_LEN] = 0;
    ipc->regs[IPC_REG_READ_LEN] = 0;
}

static int ipc_writeall(int sock, const void* data, uint32_t len)
{
    const char* ptr = (const char*)data;
    while (len > 0)
    {
        ssize_t ret = send(sock, ptr, len, 0);
        if (ret < 0)
            return -1;
        ptr += ret;
        len -= ret;
    }
    return 0;
}

static int ipc_writemsg(int sock, void* dram, uint32_t addr, uint32_t len)
{
    char buf[256];
    uint32_t l;

    uint32_t msglen = len;
    if (ipc_writeall(sock, &msglen, sizeof(msglen)) < 0)
        return -1;

    while (len)
    {
        l = len;
        if (l > sizeof(buf))
            l = sizeof(buf);
        for (int i = 0; i < l; ++i)
            buf[i] = ((char*)dram)[(addr + i) ^ 3];
        if (ipc_writeall(sock, buf, l) < 0)
            return -1;
        addr += l;
        len -= l;
    }

    return 0;
}

static int ipc_readuntil(int sock, void* data, uint32_t len)
{
    char* ptr = (char*)data;
    while (len > 0)
    {
        ssize_t ret = recv(sock, ptr, len, 0);
        if (ret < 0)
            return -1;
        if (ret == 0)
            return -1;
        ptr += ret;
        len -= ret;
    }
    return 0;
}

static int ipc_readmsg(int sock, void* dram, uint32_t addr, uint32_t* len)
{
    char buf[256];
    uint32_t msglen;
    uint32_t remain;
    uint32_t l;

    if (ipc_readuntil(sock, &msglen, sizeof(msglen)) < 0)
        return -1;
    
    if (msglen > *len)
        return -1;
    
    remain = msglen;
    while (remain)
    {
        l = remain;
        if (l > sizeof(buf))
            l = sizeof(buf);
        if (ipc_readuntil(sock, buf, l) < 0)
            return -1;
        for (int i = 0; i < l; ++i)
            ((char*)dram)[(addr + i) ^ 3] = buf[i];
        addr += l;
        remain -= l;
    }
    *len = msglen;
    return 0;
}

static void ipc_dowrite(struct ipc* ipc)
{
    uint32_t len;
    struct r4300_core* r4300 = &g_dev.r4300;
    char* dram;
    int ret;

    len = ipc->regs[IPC_REG_WRITE_LEN];
    if (!len)
        return;
        
    if (ipc->sock_client < 0)
    {
        ipc->regs[IPC_REG_WRITE_LEN] = 0;
        return;
    }

    dram = (char*)mem_base_u32(g_dev.mem.base, MM_RDRAM_DRAM);
    if (ipc_writemsg(ipc->sock_client, dram, ipc->regs[IPC_REG_RAM_ADDR], len) < 0)
    {
        printf("ipc_dowrite: Failed to write message to socket, aborting write\n");
        ipc_error(ipc);
        return;
    }
}

static void ipc_doread(struct ipc* ipc)
{
    struct pollfd pfd;
    uint32_t len;
    struct r4300_core* r4300 = &g_dev.r4300;
    char* dram;
    int ret;

    len = ipc->regs[IPC_REG_READ_LEN];
    if (!len)
        return;

    if (ipc->sock_client < 0)
    {
        ipc->regs[IPC_REG_READ_LEN] = 0;
        return;
    }

    /* Make sure the socket is ready to read */
    pfd.fd = ipc->sock_client;
    pfd.events = POLLIN | POLLERR;
    ret = poll(&pfd, 1, 0);
    if (ret < 0 || !(pfd.revents & POLLIN))
    {
        ipc->regs[IPC_REG_READ_LEN] = 0;
        return;
    }

    if (pfd.revents & POLLERR)
    {
        printf("ipc_doread: Socket error, aborting read\n");
        ipc_error(ipc);
        return;
    }

    dram = (char*)mem_base_u32(g_dev.mem.base, MM_RDRAM_DRAM);
    if (ipc_readmsg(ipc->sock_client, dram, ipc->regs[IPC_REG_RAM_ADDR], &len) < 0)
    {
        printf("ipc_doread: Failed to read message from socket, aborting read\n");
        ipc_error(ipc);
        return;
    }

    ipc->regs[IPC_REG_READ_LEN] = len;
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
    case IPC_REG_RAM_ADDR:
        ipc->regs[IPC_REG_RAM_ADDR] = value;
        break;
    case IPC_REG_WRITE_LEN:
        ipc->regs[IPC_REG_WRITE_LEN] = value;
        ipc_dowrite(ipc);
        break;
    case IPC_REG_READ_LEN:
        ipc->regs[IPC_REG_READ_LEN] = value;
        ipc_doread(ipc);
        break;
    }
}