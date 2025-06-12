/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-03     RV           the first version
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
#include <rtdbg.h>
#include <core_mqtt_config.h>

#ifdef RT_USING_ULOG
#define MQTT_PRINT(fmt, ...) LOG_D(fmt, ##__VA_ARGS__)
#else
#define MQTT_PRINT(fmt, ...) ((void)0)
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

MQTTStatus_t mqttInit(NetworkContext_t *networkContext, MQTTEventCallback_t userCallback);
MQTTStatus_t mqttConnect(NetworkContext_t *networkContext);
MQTTStatus_t mqttSubscribe(MQTTSubscribeInfo_t *subscribeInfo);
MQTTStatus_t mqttPublish(MQTTPublishInfo_t *publishInfo);
void mqttClientTask(void *parameter);

#endif /* APPLICATIONS_FIREMQTT_PORT_MQTT_USR_API_H_ */
