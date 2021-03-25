// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/iot/az_iot_hub_client.h>
#include <azure/core/internal/az_precondition_internal.h>
#include <azure/core/internal/az_result_internal.h>


AZ_NODISCARD az_result az_iot_hub_client_property_patch_get_publish_topic(
    az_iot_hub_client const* client,
    az_span request_id,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  return az_iot_hub_client_twin_patch_get_publish_topic(
      client,
      request_id,
      mqtt_topic,
      mqtt_topic_size,
      out_mqtt_topic_length);
}

AZ_NODISCARD az_result az_iot_hub_client_property_document_get_publish_topic(
    az_iot_hub_client const* client,
    az_span request_id,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  return az_iot_hub_client_twin_document_get_publish_topic(
      client,
      request_id,
      mqtt_topic,
      mqtt_topic_size,
      out_mqtt_topic_length);
}

AZ_NODISCARD az_result az_iot_hub_client_property_parse_received_topic(
    az_iot_hub_client const* client,
    az_span received_topic,
    az_iot_hub_client_property_response* out_response)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_VALID_SPAN(received_topic, 1, false);
  _az_PRECONDITION_NOT_NULL(out_response);

  az_iot_hub_client_twin_response hub_twin_response;
  _az_RETURN_IF_FAILED(az_iot_hub_client_twin_parse_received_topic(
      client, received_topic, &hub_twin_response));

  out_response->request_id = hub_twin_response.request_id;
  out_response->response_type
      = (az_iot_hub_client_property_response_type)hub_twin_response.response_type;
  out_response->status = hub_twin_response.status;
  out_response->version = hub_twin_response.version;

  return AZ_OK;
}
    

