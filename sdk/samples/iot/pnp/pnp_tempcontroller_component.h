// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef PNP_TEMPCONTROLLER_COMPONENT_H
#define PNP_TEMPCONTROLLER_COMPONENT_H

#include <azure/az_core.h>
#include "pnp_mqtt_message.h"

bool pnp_temp_controller_process_command_request(
    az_iot_hub_client const* hub_client,
    MQTTClient mqtt_client,
    az_iot_hub_client_command_request const* command_request,
    az_span command_received_payload);

void pnp_temp_controller_send_serial_number(
    az_iot_hub_client const* hub_client,
    MQTTClient mqtt_client);

void pnp_temp_controller_send_telemetry_message(az_iot_hub_client const* hub_client, MQTTClient mqtt_client);


#endif // PNP_TEMPCONTROLLER_COMPONENT_H
