/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-03     RTT       the first version
 */
#ifndef APPLICATIONS_FIREMQTT_PORT_MQTT_USR_API_H_
#define APPLICATIONS_FIREMQTT_PORT_MQTT_USR_API_H_
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

#define MQTT_BROKER_ADDRESS  "broker.emqx.io" // MQTT 代理地址
#define MQTT_BROKER_PORT     1883                 // MQTT 代理端口
#define MQTT_CLIENT_ID       "rtthread_mqtt_client" // 客户端 ID
#define MQTT_TOPIC_SUB       "rtthread/test/sub"   // 订阅主题
#define MQTT_TOPIC_PUB       "rtthread/test/pub"   // 发布主题
#define MQTT_KEEP_ALIVE      60

#define MAX_RETRY_ATTEMPTS   5                     // 最大重试次数
#define INITIAL_BACKOFF_MS   1000                  // 初始重连退避时间（毫秒）
#define MAX_BACKOFF_MS       60000

MQTTStatus_t mqttInit(NetworkContext_t *networkContext, MQTTEventCallback_t userCallback);
MQTTStatus_t mqttConnect(NetworkContext_t *networkContext);
MQTTStatus_t mqttSubscribe(void);
MQTTStatus_t mqttPublish(const char *payload);
bool isSocketReadable(int socket, int timeout_ms);

#endif /* APPLICATIONS_FIREMQTT_PORT_MQTT_USR_API_H_ */
