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

#include "mqtt_api.h"

static MQTTFixedBuffer_t mqttBuffer = { .pBuffer = RT_NULL, .size = MQTT_BUF_SIZE };
static MQTTContext_t mqttContext;
static TransportInterface_t transportInterface;
static NetworkContext_t networkContext;
static MQTTPubAckInfo_t outgoingPublishes[MQTT_OUTGOING_PUBLISH_COUNT];

MQTTStatus_t mqttInit(NetworkContext_t *networkContext, MQTTEventCallback_t userCallback)
{
    MQTTStatus_t status;

    transportInterface.pNetworkContext = networkContext;
    transportInterface.send = transportSend;
    transportInterface.recv = transportRecv;

    mqttBuffer.pBuffer = rt_malloc(mqttBuffer.size);
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
    else
    {
        status = MQTT_InitStatefulQoS(&mqttContext, outgoingPublishes, MQTT_OUTGOING_PUBLISH_COUNT, NULL, 0);
        MQTT_PRINT("MQTT client initialized successfully\n");
    }

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

    rt_kprintf("[%d] MQTT broker connected\n", getCurrentTime());
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

const char *mqttStatus(MQTTStatus_t status)
{
    const char *const statusStrings[] = {
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
            break;
    }
}

void mqttClientTask(void *parameter)
{
    MQTTStatus_t status;
    uint32_t retryCount = 0;
    uint32_t backoffMs = INITIAL_BACKOFF_MS;
    bool isConnected = false;

    if (mqttInit(&networkContext, MQTT_USER_CALLBACK) != MQTTSuccess)
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
            status = MQTT_ProcessLoop(&mqttContext);
            if (status != MQTTSuccess && status != MQTTNeedMoreBytes)
            {
                MQTT_PRINT("MQTT_ProcessLoop failed: %d (%s)\n", status, mqttStatus(status));
                status = MQTT_Disconnect(&mqttContext);
                break;
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
