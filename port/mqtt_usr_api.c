/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-03     RTT       the first version
 */

#include "mqtt_usr_api.h"

MQTTFixedBuffer_t mqttBuffer = { .pBuffer = RT_NULL, .size = 1024 };
static TransportInterface_t transportInterface;
MQTTContext_t mqttContext;

MQTTStatus_t mqttInit(NetworkContext_t *networkContext, MQTTEventCallback_t userCallback)
{
    MQTTStatus_t status;

    /* 配置传输接口 */
    transportInterface.pNetworkContext = networkContext;
    transportInterface.send = transportSend;
    transportInterface.recv = transportRecv;

    /* 初始化 MQTT 缓冲区 */
    mqttBuffer.pBuffer = rt_malloc(mqttBuffer.size);
    if (mqttBuffer.pBuffer == RT_NULL)
    {
        rt_kprintf("分配 MQTT 缓冲区失败\n");
        return MQTTSendFailed;
    }

    /* 初始化 MQTT 上下文 */
    status = MQTT_Init(&mqttContext, &transportInterface, getCurrentTime, userCallback, &mqttBuffer);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT_Init 失败: %d\n", status);
        rt_free(mqttBuffer.pBuffer);
        return status;
    }

    rt_kprintf("MQTT 客户端初始化成功\n");
    return MQTTSuccess;
}

MQTTStatus_t mqttConnect(NetworkContext_t *networkContext)
{
    MQTTStatus_t status;
    MQTTConnectInfo_t connectInfo = { 0 };
    bool sessionPresent;

    /* 配置连接信息 */
    connectInfo.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connectInfo.pClientIdentifier = MQTT_CLIENT_ID;
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE;
    connectInfo.cleanSession = true;

    /* 建立 TCP 连接 */
    networkContext->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (networkContext->socket < 0)
    {
        rt_kprintf("创建 socket 失败\n");
        return MQTTSendFailed;
    }

    struct sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(MQTT_BROKER_PORT);
    struct hostent *host = gethostbyname(MQTT_BROKER_ADDRESS);
    if (host == NULL || host->h_addr_list[0] == NULL)
    {
        rt_kprintf("解析代理地址失败\n");
        closesocket(networkContext->socket);
        return MQTTSendFailed;
    }
    serverAddr.sin_addr.s_addr = *(uint32_t *) host->h_addr_list[0];

    if (connect(networkContext->socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        rt_kprintf("连接代理失败\n");
        closesocket(networkContext->socket);
        return MQTTSendFailed;
    }

    /* MQTT 连接 */
    status = MQTT_Connect(&mqttContext, &connectInfo, NULL, 10000, &sessionPresent);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT_Connect 失败: %d\n", status);
        closesocket(networkContext->socket);
        return status;
    }

    rt_kprintf("成功连接到 MQTT 代理\n");
    return MQTTSuccess;
}

MQTTStatus_t mqttSubscribe(void)
{
    MQTTStatus_t status;
    MQTTSubscribeInfo_t subscribeInfo = { 0 };

    subscribeInfo.qos = MQTTQoS0;
    subscribeInfo.pTopicFilter = MQTT_TOPIC_SUB;
    subscribeInfo.topicFilterLength = strlen(MQTT_TOPIC_SUB);

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Subscribe(&mqttContext, &subscribeInfo, 1, packetId);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT_Subscribe 失败: %d\n", status);
        return status;
    }

    rt_kprintf("订阅主题: %s\n", MQTT_TOPIC_SUB);
    return MQTTSuccess;
}

MQTTStatus_t mqttPublish(const char *payload)
{
    MQTTStatus_t status;
    MQTTPublishInfo_t publishInfo = { 0 };

    publishInfo.qos = MQTTQoS0;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = strlen(payload);

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Publish(&mqttContext, &publishInfo, packetId);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT_Publish 失败: %d\n", status);
        return status;
    }

    rt_kprintf("发布消息: %s\n", payload);
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
