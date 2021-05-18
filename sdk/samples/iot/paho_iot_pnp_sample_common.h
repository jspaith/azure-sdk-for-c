// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef PAHO_IOT_PNP_SAMPLE_COMMON
#define PAHO_IOT_PNP_SAMPLE_COMMON

#define MQTT_TIMEOUT_RECEIVE_MAX_MESSAGE_COUNT 100
#define MQTT_TIMEOUT_RECEIVE_MS (8 * 1000)
#define MQTT_TIMEOUT_DISCONNECT_MS (10 * 1000)


extern MQTTClient mqtt_client;
extern az_iot_hub_client hub_client;

void paho_iot_pnp_sample_device_implement(void);

#endif // PAHO_IOT_PNP_SAMPLE_COMMON

