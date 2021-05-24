// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef PNP_DEVICE_INFO_COMPONENT_H
#define PNP_DEVICE_INFO_COMPONENT_H

#include <azure/az_core.h>
#include "pnp_mqtt_message.h"

/**
 * @brief Build the reported property payload to send for device info.
 *
 * @param[in] hub_client An #az_iot_hub_client corresponding to this MQTT connection.
 * @param[in] mqtt_client An MQTTClient corresponding to this MQTT connection.
 */
void pnp_device_info_send_reported_properties(
    az_iot_hub_client* hub_client, 
    MQTTClient mqtt_client);

#endif // PNP_DEVICE_INFO_COMPONENT_H
