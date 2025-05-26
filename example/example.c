/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-26     RTT       the first version
 */

#include <rtthread.h>
#include <core_mqtt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "port.h"

MQTTStatus_t MQTTStatus;

static NetworkContext_t networkContext;
static MQTTContext_t mqttContext;
static TransportInterface_t transport;
static MQTTFixedBuffer_t networkBuffer;
static uint8_t buffer[2048];

void mqtt_event_callback(MQTTContext_t * pContext, MQTTPacketInfo_t * pPacketInfo, MQTTDeserializedInfo_t * pDeserializedInfo) {
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH) {
        rt_kprintf("Received message on topic %.*s: %.*s\n",
                   pDeserializedInfo->pPublishInfo->topicNameLength,
                   pDeserializedInfo->pPublishInfo->pTopicName,
                   pDeserializedInfo->pPublishInfo->payloadLength,
                   pDeserializedInfo->pPublishInfo->pPayload);
    }
}

static void mqtt_task(void * parameter) {

    const char * broker_host = "broker.emqx.io";
    struct hostent * host = gethostbyname(broker_host);
    if (host == NULL) {
        rt_kprintf("Failed to resolve %s\n", broker_host);
        return;
    }

    char * broker_ip = inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
    rt_kprintf("Resolved to IP: %s\n", broker_ip);

    struct sockaddr_in server_addr;
    networkContext.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (networkContext.socket < 0) {
        rt_kprintf("Socket creation failed: %d\n", errno);
        return;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1883); // EMQX 的 TCP 端口
    inet_aton(broker_ip, &server_addr.sin_addr);
    if (connect(networkContext.socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        rt_kprintf("Socket connect failed: %d\n", errno);
        close(networkContext.socket);
        return;
    }

    // 配置传输接口和缓冲区
    networkBuffer.pBuffer = buffer;
    networkBuffer.size = sizeof(buffer);
    transport.pNetworkContext = &networkContext;
    transport.send = rtthread_send;
    transport.recv = rtthread_recv;

    // 初始化 MQTT
    RT_ASSERT(MQTT_Init(&mqttContext, &transport, getCurrentTime, mqtt_event_callback, &networkBuffer) == MQTTSuccess);

    // 连接到 MQTT 代理
    MQTTConnectInfo_t connectInfo = {
        .pClientIdentifier = "rtthread-client",
        .clientIdentifierLength = strlen("rtthread-client"),
        .keepAliveSeconds = 60,
        .pUserName = NULL, // EMQX 公共服务器不需要认证
        .pPassword = NULL,
        .userNameLength = 0,
        .passwordLength = 0
    };
    bool sessionPresent;
    if (MQTT_Connect(&mqttContext, &connectInfo, NULL, 10000, &sessionPresent) != MQTTSuccess) {
        rt_kprintf("MQTT connect failed\n");
        close(networkContext.socket);
        return;
    }

    // 订阅主题
    MQTTSubscribeInfo_t subscribeInfo = {
        .pTopicFilter = "rtthread/topic/sub",
        .topicFilterLength = strlen("rtthread/topic/sub"),
        .qos = MQTTQoS0
    };
    if (MQTT_Subscribe(&mqttContext, &subscribeInfo, 1, MQTT_GetPacketId(&mqttContext)) != MQTTSuccess) {
        rt_kprintf("MQTT subscribe failed\n");
        close(networkContext.socket);
        return;
    }

    // 发布测试消息
    MQTTPublishInfo_t publishInfo = {
        .qos = MQTTQoS0,
        .pTopicName = "rtthread/topic/pub",
        .topicNameLength = strlen("rtthread/topic/pub"),
        .pPayload = "Hello from RT-Thread!",
        .payloadLength = strlen("Hello from RT-Thread!")
    };

    MQTTStatus = MQTT_Publish(&mqttContext, &publishInfo, MQTT_GetPacketId(&mqttContext));

    if (MQTTStatus != MQTTSuccess)
    {
        rt_kprintf("MQTT publish failed %d\n", MQTTStatus);
    }

    int retry_delay = 1000;
    while (1) {
        MQTTStatus_t status = MQTT_ProcessLoop(&mqttContext);
        if (status != MQTTSuccess) {
            rt_kprintf("MQTT process loop failed: %d\n", status);

            close(networkContext.socket);
            networkContext.socket = socket(AF_INET, SOCK_STREAM, 0);
            if (networkContext.socket < 0) {
                rt_kprintf("Socket creation failed: %d\n", errno);
                rt_thread_mdelay(retry_delay);
                retry_delay = min(retry_delay * 2, 8000); // 使用 min 宏
                continue;
            }
            if (connect(networkContext.socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                rt_kprintf("Socket reconnect failed: %d\n", errno);
                close(networkContext.socket);
                rt_thread_mdelay(retry_delay);
                retry_delay = min(retry_delay * 2, 8000); // 使用 min 宏
                continue;
            }
            if (MQTT_Connect(&mqttContext, &connectInfo, NULL, 90000, &sessionPresent) == MQTTSuccess) {
                if (MQTT_Subscribe(&mqttContext, &subscribeInfo, 1, MQTT_GetPacketId(&mqttContext)) != MQTTSuccess) {
                    rt_kprintf("MQTT resubscribe failed\n");
                }
            }
        }
        rt_thread_mdelay(100);
    }
}

int mqtt_init(void)
{
    rt_thread_t mqtt_thread = rt_thread_create("MQTT",
                                               mqtt_task,
                                               RT_NULL,
                                               4096,
                                               10,
                                               20);
    if (mqtt_thread != RT_NULL)
    {
        rt_thread_startup(mqtt_thread);
    }
    else
    {
        rt_kprintf("Failed to create MQTT thread\n");
    }
    return 0;
}
MSH_CMD_EXPORT(mqtt_init, mqtt_init);

