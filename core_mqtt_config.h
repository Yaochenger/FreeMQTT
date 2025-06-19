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

/* MQTT Broker Address */
#ifndef MQTT_BROKER_ADDRESS
#define MQTT_BROKER_ADDRESS  "broker.emqx.io"
#endif

/* MQTT Broker Port */
#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT     1883
#endif

/* MQTT Client ID */
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID       "rtthread_mqtt_client"
#endif

/* MQTT Subscription Topic */
#ifndef MQTT_TOPIC_SUB
#define MQTT_TOPIC_SUB       "rtthread/test/sub"
#endif

/* MQTT Publish Topic */
#ifndef MQTT_TOPIC_PUB
#define MQTT_TOPIC_PUB       "rtthread/test/pub"
#endif

/* MQTT Keep Alive Time (seconds) */
#ifndef MQTT_KEEP_ALIVE
#define MQTT_KEEP_ALIVE      60
#endif

/* MQTT Thread Yield Time */
#ifndef MQTT_LOOP_CNT
#define MQTT_LOOP_CNT        60
#endif

/* MQTT Receive Polling Timeout (milliseconds) */
#ifndef MQTT_RECV_POLLING_TIMEOUT_MS
#define MQTT_RECV_POLLING_TIMEOUT_MS    (0U)
#endif

/* MQTT PINGRESP Timeout (milliseconds) */
#ifndef MQTT_PINGRESP_TIMEOUT_MS
#define MQTT_PINGRESP_TIMEOUT_MS        (10000U)
#endif

/* MQTT Outgoing Publish Count */
#ifndef MQTT_OUTGOING_PUBLISH_COUNT
#define MQTT_OUTGOING_PUBLISH_COUNT     30
#endif

/* MQTT Buffer Size */
#ifndef MQTT_BUF_SIZE
#define MQTT_BUF_SIZE                   4096
#endif

/* Maximum Retry Attempts */
#ifndef MAX_RETRY_ATTEMPTS
#define MAX_RETRY_ATTEMPTS              5
#endif

/* Initial Backoff Time (milliseconds) */
#ifndef INITIAL_BACKOFF_MS
#define INITIAL_BACKOFF_MS              1000
#endif

/* Maximum Backoff Time (milliseconds) */
#ifndef MAX_BACKOFF_MS
#define MAX_BACKOFF_MS                  60000
#endif

/* MQTT User Callback */
#ifndef MQTT_USER_CALLBACK
#define MQTT_USER_CALLBACK               mqttEventCallback
#endif

#endif /* APPLICATIONS_FIREMQTT_PORT_CONFIG_H_ */
