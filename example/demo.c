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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void mqttEventCallback(MQTTContext_t *pContext, MQTTPacketInfo_t *pPacketInfo,
        MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH)
    {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
        rt_kprintf("收到主题 %.*s 的消息: %.*s\n", pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
                pPublishInfo->payloadLength, (const char *) pPublishInfo->pPayload);
    }
    else if (pPacketInfo->type == MQTT_PACKET_TYPE_SUBACK)
    {
        rt_kprintf("订阅确认\n");
    }
    else if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBACK)
    {
        rt_kprintf("发布确认\n");
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
        rt_kprintf("MQTT 初始化失败\n");
        return;
    }

    while (1)
    {
        if (mqttConnect(&networkContext) != MQTTSuccess)
        {
            if (retryCount++ >= MAX_RETRY_ATTEMPTS)
            {
                rt_kprintf("达到最大重试次数，放弃重连\n");
                break;
            }
            rt_kprintf("将在 %d 毫秒后重试连接\n", backoffMs);
            rt_thread_mdelay(backoffMs);
            backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
            continue;
        }

        /* 重置重连参数 */
        retryCount = 0;
        backoffMs = INITIAL_BACKOFF_MS;
        lastPublishTime = getCurrentTime();

        /* 订阅主题 */
        if (mqttSubscribe() != MQTTSuccess)
        {
            closesocket(networkContext.socket);
            continue;
        }

        /* 主循环：处理消息和发布 */
        while (1)
        {
            uint32_t currentTime = getCurrentTime();

            /* 检查是否有数据可读 */
            if (isSocketReadable(networkContext.socket, 100))
            {
                /* 处理传入的 MQTT 消息 */
                status = MQTT_ProcessLoop(&mqttContext);
                if (status != MQTTSuccess)
                {
                    rt_kprintf("MQTT_ProcessLoop 失败: %d\n", status);
                    break;
                }
            }

            /* 定期发布消息 */
            if (currentTime - lastPublishTime >= publishIntervalMs)
            {
                static int counter = 0;
                char payload[64];
                rt_sprintf(payload, "来自 RT-Thread 的消息: %d", counter++);
                if (mqttPublish(payload) != MQTTSuccess)
                {
                    rt_kprintf("发布失败，准备重连\n");
                    break;
                }
                lastPublishTime = currentTime;
            }

            /* 短暂延时，避免CPU占用过高 */
            rt_thread_mdelay(50);
        }

        /* 关闭套接字 */
        if (networkContext.socket >= 0)
        {
            closesocket(networkContext.socket);
            networkContext.socket = -1;
        }

        /* 准备重连 */
        rt_kprintf("MQTT 连接断开，准备重连\n");
        rt_thread_mdelay(backoffMs);
        backoffMs = MIN(backoffMs * 2, MAX_BACKOFF_MS);
    }

    /* 清理资源 */
    if (mqttBuffer.pBuffer != RT_NULL)
    {
        rt_free(mqttBuffer.pBuffer);
        mqttBuffer.pBuffer = RT_NULL;
    }
    rt_kprintf("MQTT 客户端退出\n");
}

/* 启动 MQTT 客户端线程 */
void mqtt_client_start(void)
{
    rt_thread_t tid = rt_thread_create("mqtt", mqttClientTask,
    RT_NULL, 4096, /* 栈大小 */
    10, /* 优先级 */
    20); /* 时间片 */
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        rt_kprintf("MQTT 客户端线程启动\n");
    }
    else
    {
        rt_kprintf("创建 MQTT 客户端线程失败\n");
    }
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(mqtt_client_start, 启动 MQTT 客户端);
#endif
