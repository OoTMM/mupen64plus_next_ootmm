#include <unistd.h>
#include <stdio.h>
#include "ipc.h"

static int create_ipc_socket(void)
{
    const char* rt_dir;
    char buffer[512];

    /* Get the path to the socket */
    rt_dir = getenv("XDG_RUNTIME_DIR");
    if (rt_dir == NULL)
        return -1;
    
    snprintf(buffer, sizeof(buffer), "%s/n64-ipc")
}

void init_ipc(struct ipc* ipc)
{
    ipc->sock_listen = -1;
    ipc->sock_client = -1;
}

void poweron_ipc(struct ipc* ipc)
{
    close_ipc(ipc);
}

void close_ipc(struct ipc* ipc)
{
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
}