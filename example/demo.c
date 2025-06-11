#include <rtthread.h>
#include <core_mqtt.h>
#include <transport_interface.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include "port.h"
#include <sys/select.h>  // 添加select相关定义
#include <unistd.h>      // 添加close等系统调用定义
#include "port.h"
#include "mqtt_usr_api.h"

static NetworkContext_t networkContext;
extern MQTTFixedBuffer_t mqttBuffer;
extern MQTTContext_t mqttContext;

static void mqttEventCallback(MQTTContext_t *pContext, MQTTPacketInfo_t *pPacketInfo,
        MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (!pContext || !pPacketInfo)
    {
        rt_kprintf("Error: Invalid context or packet info\n");
        return;
    }

    switch (pPacketInfo->type)
    {
        case MQTT_PACKET_TYPE_PUBLISH:
        {
            if (!pDeserializedInfo || !pDeserializedInfo->pPublishInfo)
            {
                rt_kprintf("Error: Invalid publish info\n");
                return;
            }
            MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
            rt_kprintf("Received message on topic '%.*s': %.*s\n", pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
                    pPublishInfo->payloadLength, (const char *) pPublishInfo->pPayload);
            break;
        }
        case MQTT_PACKET_TYPE_SUBACK:
            rt_kprintf("Subscription ACK\n");
            break;
        case MQTT_PACKET_TYPE_PUBACK:
            rt_kprintf("Publish ACK\n"); // QoS0 messages do not trigger this callback.
            break;
        default:
            rt_kprintf("Unhandled packet type: %d\n", pPacketInfo->type);
            break;
    }
}

static void mqttClientTask(void *parameter)
{
    MQTTStatus_t status;
    uint32_t retryCount = 0;
    uint32_t backoffMs = INITIAL_BACKOFF_MS;
    uint32_t lastPublishTime = 0;
    const uint32_t publishIntervalMs = 5000;

    if (mqttInit(&networkContext, mqttEventCallback) != MQTTSuccess)
    {
        rt_kprintf("MQTT initialization failed\n");
        return;
    }

    while (1)
    {
        if (networkContext.socket >= 0)
        {
            closesocket(networkContext.socket);
            networkContext.socket = -1;
        }

        if (mqttConnect(&networkContext) != MQTTSuccess)
        {
            if (retryCount++ >= MAX_RETRY_ATTEMPTS)
            {
                rt_kprintf("Maximum retry attempts reached, resetting retry count after 60s\n");
                rt_thread_mdelay(60000);
                retryCount = 0;
                backoffMs = INITIAL_BACKOFF_MS;
                continue;
            }
            rt_kprintf("Connection failed (MQTTServerRefused), retrying in %d ms\n", backoffMs);
            rt_thread_mdelay(backoffMs);
            backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
            continue;
        }

        retryCount = 0;
        backoffMs = INITIAL_BACKOFF_MS;
        lastPublishTime = getCurrentTime();

        MQTTSubscribeInfo_t subscribeInfo = {
            .qos = MQTTQoS0,
            .pTopicFilter = MQTT_TOPIC_SUB,
            .topicFilterLength = strlen(MQTT_TOPIC_SUB)
        };

        if (mqttSubscribe(&subscribeInfo) != MQTTSuccess)
        {
            rt_kprintf("Subscription failed\n");
            closesocket(networkContext.socket);
            continue;
        }

        while (1)
        {
            uint32_t currentTime = getCurrentTime();

            if (isSocketReadable(networkContext.socket, 100))
            {
                status = MQTT_ProcessLoop(&mqttContext);
                if (status != MQTTSuccess)
                {
                    rt_kprintf("MQTT_ProcessLoop failed: %d (%s)\n", status,
                               status == MQTTRecvFailed ? "Receive failed" :
                               status == MQTTBadResponse ? "Bad response" : "Other error");
                    break;
                }
            }

            if (currentTime - lastPublishTime >= publishIntervalMs)
            {
                static int counter = 0;
                char payload[64];
                rt_sprintf(payload, "[%d]Message from RT-Thread: Hello World", counter++);

                MQTTPublishInfo_t publishInfo = {
                    .qos = MQTTQoS0,
                    .pTopicName = MQTT_TOPIC_PUB,
                    .topicNameLength = strlen(MQTT_TOPIC_PUB),
                    .pPayload = payload,
                    .payloadLength = strlen(payload)
                };

                if (mqttPublish(&publishInfo) != MQTTSuccess)
                {
                    rt_kprintf("Publish failed, preparing to reconnect\n");
                    break;
                }
                lastPublishTime = currentTime;
            }

            rt_thread_mdelay(50);
        }

        if (networkContext.socket >= 0)
        {
            closesocket(networkContext.socket);
            networkContext.socket = -1;
        }

        rt_kprintf("MQTT connection lost, preparing to reconnect in %d ms\n", backoffMs);
        rt_thread_mdelay(backoffMs);
        backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
    }

    if (mqttBuffer.pBuffer != RT_NULL)
    {
        rt_free(mqttBuffer.pBuffer);
        mqttBuffer.pBuffer = RT_NULL;
    }
    rt_kprintf("MQTT client exited\n");
}

void mqtt_client_start(void)
{
    rt_thread_t tid = rt_thread_create("mqtt", mqttClientTask,
                                               RT_NULL, 4096,
                                               10,
                                               20);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        rt_kprintf("MQTT client thread started\n");
    }
    else
    {
        rt_kprintf("Failed to create MQTT client thread\n");
    }
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(mqtt_client_start, 启动 MQTT 客户端);
#endif
