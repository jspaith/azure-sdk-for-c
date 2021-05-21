// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <stddef.h>

#include <azure/az_core.h>

#include <azure/iot/az_iot_hub_client_properties.h>
#include <iot_sample_common.h>

#include "pnp_tempcontroller_component.h"

static az_span const command_reboot_name = AZ_SPAN_LITERAL_FROM_STR("reboot");
static az_span const command_empty_response_payload = AZ_SPAN_LITERAL_FROM_STR("{}");

bool pnp_temp_controller_process_command_request(
    az_iot_hub_client const* hub_client,
    MQTTClient mqtt_client,
    az_iot_hub_client_command_request const* command_request,
    az_span command_received_payload)
{
  (void)command_received_payload; // Not used

  pnp_mqtt_message publish_message;
  pnp_mqtt_message_init(&publish_message);

  az_iot_status status;

  if (az_span_is_content_equal(command_reboot_name, command_request->command_name))
  {
    // Simulate invocation of command.
    IOT_SAMPLE_LOG("Client invoking reboot command on Temperature Controller.");
    publish_message.out_payload = command_empty_response_payload;
    status = AZ_IOT_STATUS_OK;
  }
  else // Unsupported command
  {
    IOT_SAMPLE_LOG_AZ_SPAN("Command not supported on Temperature Controller:", command_request->command_name);
    publish_message.out_payload = command_empty_response_payload;
    status = AZ_IOT_STATUS_NOT_FOUND;
  }

  // Get the commands response topic to publish the command response.
  az_result rc = az_iot_hub_client_commands_response_get_publish_topic(
      hub_client,
      command_request->request_id,
      (uint16_t)status,
      publish_message.topic,
      publish_message.topic_length,
      NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get the commands response topic");

  // Publish the command response.
  publish_mqtt_message(
      mqtt_client, publish_message.topic, publish_message.out_payload, IOT_SAMPLE_MQTT_PUBLISH_QOS);
  IOT_SAMPLE_LOG_SUCCESS("Client published command response.");
  IOT_SAMPLE_LOG("Status: %d", status);
  IOT_SAMPLE_LOG_AZ_SPAN("Payload:", publish_message.out_payload);

  return true;
}


