#include <rtthread.h>
#include <core_mqtt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "port.h"

uint32_t getCurrentTime(void)
{
    return rt_tick_get() / (1000 / RT_TICK_PER_SECOND);
}

int32_t transportSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend)
{
    return send(pNetworkContext->socket, pBuffer, bytesToSend, 0);
}

int32_t transportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRead)
{
    return recv(pNetworkContext->socket, pBuffer, bytesToRead, 0);
}
