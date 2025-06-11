/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-03     RV          the first version
 */

#define DBG_TAG "MQTT"
#define DBG_LVL DBG_LOG

#include "mqtt_usr_api.h"

MQTTFixedBuffer_t mqttBuffer = { .pBuffer = RT_NULL, .size = 1024 };
MQTTContext_t mqttContext;
TransportInterface_t transportInterface;

MQTTStatus_t mqttInit(NetworkContext_t *networkContext, MQTTEventCallback_t userCallback)
{
    MQTTStatus_t status;

    transportInterface.pNetworkContext = networkContext;
    transportInterface.send = transportSend;
    transportInterface.recv = transportRecv;

    mqttBuffer.pBuffer = rt_malloc(mqttBuffer.size); // 缓存
    if (mqttBuffer.pBuffer == RT_NULL)
    {
        MQTT_PRINT("Failed to allocate MQTT buffer\n");
        return MQTTNoMemory;
    }

    status = MQTT_Init(&mqttContext, &transportInterface, getCurrentTime, userCallback, &mqttBuffer);
    if (status != MQTTSuccess)
    {
        MQTT_PRINT("MQTT_Init failed: %d\n", status);
        rt_free(mqttBuffer.pBuffer);
        return status;
    }

    MQTT_PRINT("MQTT client initialized successfully\n");
    return MQTTSuccess;
}

MQTTStatus_t mqttConnect(NetworkContext_t *networkContext)
{
    MQTTStatus_t status;
    MQTTConnectInfo_t connectInfo = { 0 };
    bool sessionPresent;

    /* Configure connection information */
    connectInfo.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connectInfo.pClientIdentifier = MQTT_CLIENT_ID;
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE;
    connectInfo.cleanSession = true;

    /* Establish TCP connection */
    networkContext->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (networkContext->socket < 0)
    {
        MQTT_PRINT("Failed to create socket\n");
        return MQTTSendFailed;
    }
    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(MQTT_BROKER_PORT);
    struct hostent *host = gethostbyname(MQTT_BROKER_ADDRESS);
    if (host == NULL || host->h_addr_list[0] == NULL)
    {
        MQTT_PRINT("Failed to resolve broker address\n");
        closesocket(networkContext->socket);
        return MQTTSendFailed;
    }
    serverAddr.sin_addr.s_addr = *(uint32_t *) host->h_addr_list[0];

    if (connect(networkContext->socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        MQTT_PRINT("Failed to connect to broker\n");
        closesocket(networkContext->socket);
        return MQTTSendFailed;
    }

    /* MQTT connection */
    status = MQTT_Connect(&mqttContext, &connectInfo, NULL, 10000, &sessionPresent);
    if ((status != MQTTSuccess) && (status != MQTTStatusConnected))
    {
        MQTT_PRINT("MQTT_Connect failed: %d\n", status);
        closesocket(networkContext->socket);
        return status;
    }

    rt_kprintf("MQTT broker connected\n");
    return MQTTSuccess;
}

MQTTStatus_t mqttSubscribe(MQTTSubscribeInfo_t *subscribeInfo)
{
    MQTTStatus_t status;

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Subscribe(&mqttContext, subscribeInfo, 1, packetId);
    if (status != MQTTSuccess)
    {
        MQTT_PRINT("MQTT_Subscribe failed: %d\n", status);
        return status;
    }

    MQTT_PRINT("Subscribed to topic: %s\n", MQTT_TOPIC_SUB);
    return MQTTSuccess;
}

MQTTStatus_t mqttPublish(MQTTPublishInfo_t *publishInfo)
{
    MQTTStatus_t status;

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Publish(&mqttContext, publishInfo, packetId);
    if (status != MQTTSuccess)
    {
        MQTT_PRINT("MQTT_Publish failed: %d\n", status);
        return status;
    }

    MQTT_PRINT("Published message: %s\n", publishInfo->pPayload);
    return MQTTSuccess;
}

bool isSocketReadable(int socket, int timeout_ms)
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(socket + 1, &readfds, NULL, NULL, &timeout);
    return (result > 0 && FD_ISSET(socket, &readfds));
}

const char *mqttStatus(MQTTStatus_t status)
{
    static const char *const statusStrings[] = {
        "Success",
        "BadParameter",
        "NoMemory",
        "SendFailed",
        "RecvFailed",
        "BadResponse",
        "ServerRefused",
        "NoDataAvailable",
        "IllegalState",
        "StateCollision",
        "KeepAliveTimeout",
        "NeedMoreBytes",
        "Connected",
        "NotConnected",
        "DisconnectPending",
        "PublishStoreFailed",
        "PublishRetrieveFailed"
    };

    if (status >= 0 && status < sizeof(statusStrings) / sizeof(statusStrings[0]))
    {
        return statusStrings[status];
    }
    return "Unknown";
}
