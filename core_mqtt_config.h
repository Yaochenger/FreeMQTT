/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-10     RV          the first version
 */
#ifndef APPLICATIONS_FIREMQTT_PORT_CONFIG_H_
#define APPLICATIONS_FIREMQTT_PORT_CONFIG_H_
#include "mqtt_api.h"
#define MQTT_BROKER_ADDRESS  "broker.emqx.io"       // MQTT 代理地址
#define MQTT_BROKER_PORT     1883                   // MQTT 代理端口
#define MQTT_CLIENT_ID       "rtthread_mqtt_client" // 客户端 ID
#define MQTT_TOPIC_SUB       "rtthread/test/sub"    // 订阅主题
#define MQTT_TOPIC_PUB       "rtthread/test/pub"    // 发布主题
#define MQTT_KEEP_ALIVE      60
#define MQTT_LOOP_CNT        60
#define MQTT_RECV_POLLING_TIMEOUT_MS    ( 0U )
#define MQTT_PINGRESP_TIMEOUT_MS    ( 10000U )
#define MQTT_OUTgoing_PublishCount 30

#define MAX_RETRY_ATTEMPTS   5                      // 最大重试次数
#define INITIAL_BACKOFF_MS   1000                   // 初始重连退避时间（毫秒）
#define MAX_BACKOFF_MS       60000
//#define LogDebug(args) do { rt_kprintf args; rt_kprintf("\n"); } while (0)

#define MQTT_USERCALLBACK mqttEventCallback

#endif /* APPLICATIONS_FIREMQTT_PORT_CONFIG_H_ */
