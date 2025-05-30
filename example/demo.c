#include <rtthread.h>
#include <core_mqtt.h>
#include <transport_interface.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include "port.h"

/* MQTT 代理配置 */
#define MQTT_BROKER_ADDRESS  "broker.emqx.io" // MQTT 代理地址
#define MQTT_BROKER_PORT     1883                 // MQTT 代理端口
#define MQTT_CLIENT_ID       "rtthread_mqtt_client" // 客户端 ID
#define MQTT_TOPIC_SUB       "rtthread/test/sub"   // 订阅主题
#define MQTT_TOPIC_PUB       "rtthread/test/pub"   // 发布主题
#define MQTT_KEEP_ALIVE      60                    // 保持连接时间（秒）

/* 重连参数 */
#define MAX_RETRY_ATTEMPTS   5                     // 最大重试次数
#define INITIAL_BACKOFF_MS   1000                  // 初始重连退避时间（毫秒）
#define MAX_BACKOFF_MS       60000                 // 最大重连退避时间（毫秒）

/* 定义 MIN 宏 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* MQTT 上下文和传输接口 */
static MQTTContext_t mqttContext;                  // MQTT 上下文
static TransportInterface_t transportInterface;    // 传输接口
static NetworkContext_t networkContext;            // 网络上下文
static MQTTFixedBuffer_t mqttBuffer = {           // MQTT 缓冲区
    .pBuffer = RT_NULL,
    .size = 1024
};

/* 获取当前时间的函数 */
static uint32_t desk_getCurrentTime(void) {
    return rt_tick_get() / (1000 / RT_TICK_PER_SECOND); // 转换为毫秒
}

/* 网络传输接口实现：发送数据 */
static int32_t transportSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend) {
    return send(pNetworkContext->socket, pBuffer, bytesToSend, 0);
}

/* 网络传输接口实现：接收数据 */
static int32_t transportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRead) {
    return recv(pNetworkContext->socket, pBuffer, bytesToRead, 0);
}

/* MQTT 消息处理回调 */
static void mqttEventCallback(MQTTContext_t *pContext,
                             MQTTPacketInfo_t *pPacketInfo,
                             MQTTDeserializedInfo_t *pDeserializedInfo) {
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
        rt_kprintf("收到主题 %.*s 的消息: %.*s\n",
                   pPublishInfo->topicNameLength, pPublishInfo->pTopicName,
                   pPublishInfo->payloadLength, (const char *)pPublishInfo->pPayload);
    } else if (pPacketInfo->type == MQTT_PACKET_TYPE_SUBACK) {
        rt_kprintf("订阅确认\n");
    } else if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBACK) {
        rt_kprintf("发布确认\n");
    }
}

/* 初始化 MQTT 客户端 */
static MQTTStatus_t mqttInit(void) {
    MQTTStatus_t status;

    /* 配置传输接口 */
    transportInterface.pNetworkContext = &networkContext;
    transportInterface.send = transportSend;
    transportInterface.recv = transportRecv;

    /* 初始化 MQTT 缓冲区 */
    mqttBuffer.pBuffer = rt_malloc(mqttBuffer.size);
    if (mqttBuffer.pBuffer == RT_NULL) {
        rt_kprintf("分配 MQTT 缓冲区失败\n");
        return MQTTSendFailed;
    }

    /* 初始化 MQTT 上下文 */
    status = MQTT_Init(&mqttContext, &transportInterface, desk_getCurrentTime, mqttEventCallback, &mqttBuffer);
    if (status != MQTTSuccess) {
        rt_kprintf("MQTT_Init 失败: %d\n", status);
        rt_free(mqttBuffer.pBuffer);
        return status;
    }

    rt_kprintf("MQTT 客户端初始化成功\n");
    return MQTTSuccess;
}

/* 连接到 MQTT 代理 */
static MQTTStatus_t mqttConnect(void) {
    MQTTStatus_t status;
    MQTTConnectInfo_t connectInfo = {0};
    bool sessionPresent;

    /* 配置连接信息 */
    connectInfo.clientIdentifierLength = strlen(MQTT_CLIENT_ID);
    connectInfo.pClientIdentifier = MQTT_CLIENT_ID;
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE;
    connectInfo.cleanSession = true;

    /* 建立 TCP 连接 */
    networkContext.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (networkContext.socket < 0) {
        rt_kprintf("创建 socket 失败\n");
        return MQTTSendFailed;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(MQTT_BROKER_PORT);
    struct hostent *host = gethostbyname(MQTT_BROKER_ADDRESS);
    if (host == NULL || host->h_addr_list[0] == NULL) {
        rt_kprintf("解析代理地址失败\n");
        closesocket(networkContext.socket);
        return MQTTSendFailed;
    }
    serverAddr.sin_addr.s_addr = *(uint32_t *)host->h_addr_list[0];

    if (connect(networkContext.socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        rt_kprintf("连接代理失败\n");
        closesocket(networkContext.socket);
        return MQTTSendFailed;
    }

    /* MQTT 连接 */
    status = MQTT_Connect(&mqttContext, &connectInfo, NULL, 10000, &sessionPresent);
    if (status != MQTTSuccess) {
        rt_kprintf("MQTT_Connect 失败: %d\n", status);
        closesocket(networkContext.socket);
        return status;
    }

    rt_kprintf("成功连接到 MQTT 代理\n");
    return MQTTSuccess;
}

/* 订阅主题 */
static MQTTStatus_t mqttSubscribe(void) {
    MQTTStatus_t status;
    MQTTSubscribeInfo_t subscribeInfo = {0};

    subscribeInfo.qos = MQTTQoS0;
    subscribeInfo.pTopicFilter = MQTT_TOPIC_SUB;
    subscribeInfo.topicFilterLength = strlen(MQTT_TOPIC_SUB);

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Subscribe(&mqttContext, &subscribeInfo, 1, packetId);
    if (status != MQTTSuccess) {
        rt_kprintf("MQTT_Subscribe 失败: %d\n", status);
        return status;
    }

    rt_kprintf("订阅主题: %s\n", MQTT_TOPIC_SUB);
    return MQTTSuccess;
}

/* 发布消息 */
static MQTTStatus_t mqttPublish(const char *payload) {
    MQTTStatus_t status;
    MQTTPublishInfo_t publishInfo = {0};

    publishInfo.qos = MQTTQoS0;
    publishInfo.pTopicName = MQTT_TOPIC_PUB;
    publishInfo.topicNameLength = strlen(MQTT_TOPIC_PUB);
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = strlen(payload);

    uint16_t packetId = MQTT_GetPacketId(&mqttContext);
    status = MQTT_Publish(&mqttContext, &publishInfo, packetId);
    if (status != MQTTSuccess) {
        rt_kprintf("MQTT_Publish 失败: %d\n", status);
        return status;
    }

    rt_kprintf("发布消息: %s\n", payload);
    return MQTTSuccess;
}

/* MQTT 客户端任务 */
static void mqttClientTask(void *parameter) {
    MQTTStatus_t status;
    uint32_t retryCount = 0;
    uint32_t backoffMs = INITIAL_BACKOFF_MS;

    /* 初始化 MQTT */
    if (mqttInit() != MQTTSuccess) {
        rt_kprintf("MQTT 初始化失败\n");
        return;
    }

    while (1) {
        /* 尝试连接 */
        if (mqttConnect() != MQTTSuccess) {
            if (retryCount++ >= MAX_RETRY_ATTEMPTS) {
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

        /* 订阅主题 */
        if (mqttSubscribe() != MQTTSuccess) {
            closesocket(networkContext.socket);
            continue;
        }

        /* 主循环：处理消息和发布 */
        while (1) {
            /* 处理传入的 MQTT 消息 */
            status = MQTT_ProcessLoop(&mqttContext);
            if (status != MQTTSuccess) {
                rt_kprintf("MQTT_ProcessLoop 失败: %d\n", status);
                closesocket(networkContext.socket);
                break;
            }

            /* 定期发布消息 */
            static int counter = 0;
            char payload[64];
            rt_sprintf(payload, "来自 RT-Thread 的消息: %d", counter++);
            if (mqttPublish(payload) != MQTTSuccess) {
                rt_kprintf("发布失败，准备重连\n");
                closesocket(networkContext.socket);
                break;
            }

            rt_thread_mdelay(50); /* 每 5 秒发布一次 */
        }
    }

    /* 清理资源 */
    rt_free(mqttBuffer.pBuffer);
    rt_kprintf("MQTT 客户端退出\n");
}

/* 启动 MQTT 客户端线程 */
void mqtt_client_start(void) {
    rt_thread_t tid = rt_thread_create("mqtt",
                                      mqttClientTask,
                                      RT_NULL,
                                      4096, /* 栈大小 */
                                      10,   /* 优先级 */
                                      20);  /* 时间片 */
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
        rt_kprintf("MQTT 客户端线程启动\n");
    } else {
        rt_kprintf("创建 MQTT 客户端线程失败\n");
    }
}

#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(mqtt_client_start, 启动 MQTT 客户端);
#endif
