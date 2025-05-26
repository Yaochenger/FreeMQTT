#include <rtthread.h>
#include <core_mqtt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "port.h"

int32_t rtthread_send(NetworkContext_t * pNetworkContext, const void * pBuffer, size_t bytesToSend)
{
    int socket = pNetworkContext->socket;
    int32_t bytesSent = send(socket, pBuffer, bytesToSend, 0);
    if (bytesSent < 0) {
        rt_kprintf("Send failed: %d\n", errno);
    }
    return bytesSent;
}

int32_t rtthread_recv(NetworkContext_t * pNetworkContext, void * pBuffer, size_t bytesToRecv)
{
    int socket = pNetworkContext->socket;
    int32_t bytesReceived = recv(socket, pBuffer, bytesToRecv, 0);
    if (bytesReceived < 0) {
        rt_kprintf("Recv failed: %d\n", errno);
    }
    return bytesReceived;
}

uint32_t getCurrentTime(void)
{
    return rt_tick_get() * (1000 / RT_TICK_PER_SECOND);
}
