#include <rtthread.h>
#include <core_mqtt.h>
#include <transport_interface.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include "port.h"
#include <sys/select.h>
#include <unistd.h>
#include "port.h"

#define DBG_TAG "MQTT"
#define DBG_LVL DBG_LOG

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
            rt_kprintf("Other packet type\n");
            break;
    }
}

static void mqttClientTask(void *parameter)
{
    MQTTStatus_t status;
    uint32_t retryCount = 0;
    uint32_t backoffMs = INITIAL_BACKOFF_MS;
    bool isConnected = false;

    if (mqttInit(&networkContext, mqttEventCallback) != MQTTSuccess)
    {
        MQTT_PRINT("MQTT initialization failed\n");
        return;
    }

    while (1)
    {
        if (networkContext.socket >= 0)
        {
            closesocket(networkContext.socket);
            networkContext.socket = -1;
        }

        status = mqttConnect(&networkContext);
        if (status != MQTTSuccess)
        {
            MQTT_PRINT("Connection failed: %d (%s), retrying in %d ms\n", status, mqttStatus(status), backoffMs);
            if (retryCount++ >= MAX_RETRY_ATTEMPTS)
            {
                MQTT_PRINT("Maximum retry attempts reached, resetting retry count after 60s\n");
                rt_thread_mdelay(30000);
                retryCount = 0;
                backoffMs = INITIAL_BACKOFF_MS;
            }
            else
            {
                rt_thread_mdelay(backoffMs);
                backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
            }
            continue;
        }

        isConnected = true;
        retryCount = 0;
        backoffMs = INITIAL_BACKOFF_MS;

        while (1)
        {
            if (isSocketReadable(networkContext.socket, 100))
            {
                status = MQTT_ProcessLoop(&mqttContext);
                if (status != MQTTSuccess)
                {
                    MQTT_PRINT("MQTT_ProcessLoop failed: %d (%s)\n", status, mqttStatus(status));
                    break;
                }
            }
            rt_thread_mdelay(MQTT_LOOP_CNT);
        }

        if (isConnected && networkContext.socket >= 0)
        {
            status = MQTT_Disconnect(&mqttContext);
            if (status != MQTTSuccess)
            {
                MQTT_PRINT("MQTT_Disconnect failed: %d (%s)\n", status, mqttStatus(status));
            }
            isConnected = false;
        }

        if (networkContext.socket >= 0)
        {
            closesocket(networkContext.socket);
            networkContext.socket = -1;
        }

        MQTT_PRINT("MQTT connection lost, preparing to reconnect in %d ms\n", backoffMs);
        rt_thread_mdelay(backoffMs);
        backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
    }

    if (mqttBuffer.pBuffer != RT_NULL)
    {
        rt_free(mqttBuffer.pBuffer);
        mqttBuffer.pBuffer = RT_NULL;
    }
    MQTT_PRINT("MQTT client exited\n");
}

#include <wlan_mgnt.h>
void mqtt_client_start(void)
{
    rt_wlan_unregister_event_handler(RT_WLAN_EVT_READY);

    rt_thread_t tid = rt_thread_create("mqtt", mqttClientTask,
    RT_NULL, 4096, 10, 20);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        MQTT_PRINT("MQTT client thread started\n");
    }
    else
    {
        MQTT_PRINT("Failed to create MQTT client thread\n");
    }
}

static int mqtt_pub(int argc, char **argv)
{
    MQTTStatus_t status;
    MQTTPublishInfo_t publishInfo;

    if (argc != 2)
    {
        rt_kprintf("Usage: mqtt_pub <message>\n");
        return -RT_ERROR;
    }

    publishInfo.qos = MQTTQoS0;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = argv[1];
    publishInfo.payloadLength = strlen(argv[1]);

    status = mqttPublish(&publishInfo);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT publish failed: %d (%s)\n", status, mqttStatus(status));
        return -RT_ERROR;
    }

    rt_kprintf("Published message: %s to topic: %s\n", argv[1], MQTT_TOPIC_PUB);
    return RT_EOK;
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT_ALIAS(mqtt_pub, mqtt_pub, Send MQTT message);
#endif

static int mqtt_sub(int argc, char **argv)
{
    MQTTStatus_t status;
    MQTTSubscribeInfo_t subscribeInfo;

    if (argc != 2)
    {
        rt_kprintf("Usage: mqtt_sub <topic>\n");
        return -RT_ERROR;
    }

    subscribeInfo.qos = MQTTQoS0;
    subscribeInfo.pTopicFilter = argv[1];
    subscribeInfo.topicFilterLength = strlen(argv[1]);

    status = mqttSubscribe(&subscribeInfo);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT subscribe failed: %d (%s)\n", status, mqttStatus(status));
        return -RT_ERROR;
    }

    rt_kprintf("Subscribed to topic: %s\n", argv[1]);
    return RT_EOK;
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT_ALIAS(mqtt_sub, mqtt_sub, Subscribe MQTT message);
#endif
