#include <rtthread.h>

#define DBG_TAG "MQTT"
#define DBG_LVL DBG_LOG

#include "mqtt_api.h"

#define CORE_MQTTT_STACK_SIZE       4096
#define CORE_MQTTT_PRIORITY         10
#define CORE_MQTTT_TIMESLICE        20

void mqtt_client_start(void)
{
    rt_thread_t tid = rt_thread_create("mqtt", mqttClientTask,
                                               RT_NULL,
                                               CORE_MQTTT_STACK_SIZE,
                                               CORE_MQTTT_PRIORITY,
                                               CORE_MQTTT_TIMESLICE);
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

    publishInfo.qos = MQTTQoS2;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = argv[1];
    publishInfo.payloadLength = strlen(argv[1]);

    status = mqttPublish(&publishInfo);
    if (status != MQTTSuccess)
    {
        rt_kprintf("MQTT publish failed: %d\n", status);
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
        rt_kprintf("MQTT subscribe failed: %d\n", status);
        return -RT_ERROR;
    }

    rt_kprintf("Subscribed to topic: %s\n", argv[1]);
    return RT_EOK;
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT_ALIAS(mqtt_sub, mqtt_sub, Subscribe MQTT message);
#endif
