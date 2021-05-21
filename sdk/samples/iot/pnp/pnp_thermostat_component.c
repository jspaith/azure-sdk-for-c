// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifdef _MSC_VER
// warning C4996: 'localtime': This function or variable may be unsafe. Consider using localtime_s
// instead.
#pragma warning(disable : 4996)
#endif

#include <azure/iot/az_iot_hub_client_properties.h>

#include <iot_sample_common.h>

#include "pnp_thermostat_component.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <azure/az_core.h>
#include <azure/az_iot.h>

#include <iot_sample_common.h>

#include "pnp_mqtt_message.h"
#include "pnp_thermostat_component.h"

#define DOUBLE_DECIMAL_PLACE_DIGITS 2
#define DEFAULT_START_TEMP_COUNT 1

static char const iso_spec_time_format[] = "%Y-%m-%dT%H:%M:%SZ"; // ISO8601 Time Format

// IoT Hub Device Property Values
static az_span const property_desired_temperature_property_name
    = AZ_SPAN_LITERAL_FROM_STR("targetTemperature");
static az_span const property_reported_maximum_temperature_property_name
    = AZ_SPAN_LITERAL_FROM_STR("maxTempSinceLastReboot");
static az_span const property_response_success = AZ_SPAN_LITERAL_FROM_STR("success");

// IoT Hub Commands Values
static az_span const command_getMaxMinReport_name = AZ_SPAN_LITERAL_FROM_STR("getMaxMinReport");
static az_span const command_max_temp_name = AZ_SPAN_LITERAL_FROM_STR("maxTemp");
static az_span const command_min_temp_name = AZ_SPAN_LITERAL_FROM_STR("minTemp");
static az_span const command_avg_temp_name = AZ_SPAN_LITERAL_FROM_STR("avgTemp");
static az_span const command_start_time_name = AZ_SPAN_LITERAL_FROM_STR("startTime");
static az_span const command_end_time_name = AZ_SPAN_LITERAL_FROM_STR("endTime");
static az_span const command_empty_response_payload = AZ_SPAN_LITERAL_FROM_STR("{}");
static char command_start_time_value_buffer[32];
static char command_end_time_value_buffer[32];

// IoT Hub Telemetry Values
static az_span const telemetry_temperature_name = AZ_SPAN_LITERAL_FROM_STR("temperature");

static void build_command_response_payload(
    pnp_thermostat_component const* thermostat_component,
    az_span start_time,
    az_span end_time,
    az_span payload,
    az_span* out_payload)
{
  char const* const log = "Failed to build command response payload";

  az_json_writer jw;
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_init(&jw, payload, NULL), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_begin_object(&jw), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, command_max_temp_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(
          &jw, thermostat_component->maximum_temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, command_min_temp_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(
          &jw, thermostat_component->minimum_temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, command_avg_temp_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(
          &jw, thermostat_component->average_temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, command_start_time_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_string(&jw, start_time), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, command_end_time_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_string(&jw, end_time), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_end_object(&jw), log);

  *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);
}

static bool invoke_getMaxMinReport(
    pnp_thermostat_component const* thermostat_component,
    az_span payload,
    az_span response,
    az_span* out_response)
{
  int32_t incoming_since_value_len = 0;

  // Parse the `since` field in the payload.
  char const* const log = "Failed to parse for `since` field in payload";

  az_json_reader jr;
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_init(&jr, payload, NULL), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(&jr), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_token_get_string(
          &jr.token,
          command_start_time_value_buffer,
          sizeof(command_start_time_value_buffer),
          &incoming_since_value_len),
      log);

  // Set the response payload to error if the `since` field was empty.
  if (incoming_since_value_len == 0)
  {
    *out_response = command_empty_response_payload;
    return false;
  }

  az_span start_time_span
      = az_span_create((uint8_t*)command_start_time_value_buffer, incoming_since_value_len);

  IOT_SAMPLE_LOG_AZ_SPAN("Start Time:", start_time_span);

  // Get the current time as a string.
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  size_t length = strftime(
      command_end_time_value_buffer,
      sizeof(command_end_time_value_buffer),
      iso_spec_time_format,
      timeinfo);
  az_span end_time_span = az_span_create((uint8_t*)command_end_time_value_buffer, (int32_t)length);

  IOT_SAMPLE_LOG_AZ_SPAN("End Time:", end_time_span);

  // Build command response message.
  build_command_response_payload(
      thermostat_component, start_time_span, end_time_span, response, out_response);

  return true;
}

az_result pnp_thermostat_init(
    pnp_thermostat_component* out_thermostat_component,
    az_span component_name,
    double initial_temperature)
{
  if (out_thermostat_component == NULL)
  {
    return AZ_ERROR_ARG;
  }

  out_thermostat_component->component_name = component_name;
  out_thermostat_component->average_temperature = initial_temperature;
  out_thermostat_component->current_temperature = initial_temperature;
  out_thermostat_component->maximum_temperature = initial_temperature;
  out_thermostat_component->minimum_temperature = initial_temperature;
  out_thermostat_component->temperature_count = DEFAULT_START_TEMP_COUNT;
  out_thermostat_component->temperature_summation = initial_temperature;
  out_thermostat_component->send_maximum_temperature_property = true;

  return AZ_OK;
}

static void pnp_thermostat_build_telemetry_message(
    pnp_thermostat_component* thermostat_component,
    az_span payload,
    az_span* out_payload)
{
  az_json_writer jw;

  char const* const log = "Failed to build telemetry payload";

  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_init(&jw, payload, NULL), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_begin_object(&jw), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, telemetry_temperature_name), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(
          &jw, thermostat_component->current_temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_end_object(&jw), log);

  *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);
}

void pnp_thermostat_send_telemetry_message(pnp_thermostat_component* thermostat_component, az_iot_hub_client const* hub_client, MQTTClient mqtt_client)
{
    pnp_mqtt_message publish_message;
    pnp_mqtt_message_init(&publish_message);

    unsigned char properties_buffer[64]; // TODO: magic # in code, fix.
    az_span properties_span = az_span_create(properties_buffer, sizeof(properties_buffer));
    az_iot_message_properties properties;
    
    az_result rc = az_iot_message_properties_init(&properties, properties_span, 0);
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Unable to allocate properties");
    
    rc = az_iot_message_properties_append(
        &properties, AZ_SPAN_FROM_STR(AZ_IOT_MESSAGE_COMPONENT_NAME), thermostat_component->component_name);
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Unable to append properties");
    
    // Get the telemetry topic to publish the telemetry message.
    rc = az_iot_hub_client_telemetry_get_publish_topic(
        hub_client, &properties, publish_message.topic, publish_message.topic_length, NULL);
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Unable to get the telemetry topic");
    
    // Build the telemetry message.
    pnp_thermostat_build_telemetry_message(
        thermostat_component, publish_message.payload, &publish_message.out_payload);
    
    // Publish the telemetry message.
    publish_mqtt_message(
        mqtt_client, publish_message.topic, publish_message.out_payload, IOT_SAMPLE_MQTT_PUBLISH_QOS);
    IOT_SAMPLE_LOG_SUCCESS("Client published the telemetry message for Temperature Sensor 1.");
    IOT_SAMPLE_LOG_AZ_SPAN("Payload:", publish_message.out_payload);

}

void pnp_thermostat_build_maximum_temperature_reported_property(
    az_iot_hub_client const* hub_client,
    pnp_thermostat_component* thermostat_component,
    az_span payload,
    az_span* out_payload)
{
  az_json_writer jw;

  const char* const log = "Failed to build maximum temperature reported property payload";

  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_init(&jw, payload, NULL), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_begin_object(&jw), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_begin_component(
          hub_client, &jw, thermostat_component->component_name),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_property_name(&jw, property_reported_maximum_temperature_property_name),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(
          &jw, thermostat_component->current_temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_end_component(hub_client, &jw), log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_end_object(&jw), log);

  *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);
}


static void pnp_thermostat_update_maximum_temperature_property(pnp_thermostat_component* thermostat_component, az_iot_hub_client const* hub_client, MQTTClient mqtt_client, pnp_mqtt_message* publish_message)
{
  // Get the property PATCH topic to send a reported property update.
  az_result rc = az_iot_hub_client_properties_patch_get_publish_topic(
      hub_client,
      pnp_mqtt_get_request_id(),
      publish_message->topic,
      publish_message->topic_length,
      NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get the property PATCH topic");

  // Build the maximum temperature reported property message.
  pnp_thermostat_build_maximum_temperature_reported_property(
      hub_client,
      thermostat_component,
      publish_message->payload,
      &publish_message->out_payload);

  // Publish the maximum temperature reported property update.
  publish_mqtt_message(
      mqtt_client, publish_message->topic, publish_message->out_payload, IOT_SAMPLE_MQTT_PUBLISH_QOS);
  IOT_SAMPLE_LOG_SUCCESS(
      "Client sent Temperature Sensor the `%.*s` reported property message.",
      az_span_size(property_reported_maximum_temperature_property_name),
      az_span_ptr(property_reported_maximum_temperature_property_name));
  IOT_SAMPLE_LOG_AZ_SPAN("Payload:", publish_message->out_payload);
  IOT_SAMPLE_LOG(" "); // Formatting
}

static void pnp_thermostat_update_device_temperature_property(pnp_thermostat_component* thermostat_component, double temperature)
{
    // Update variables locally.
    thermostat_component->current_temperature = temperature;
    if (thermostat_component->current_temperature
        > thermostat_component->maximum_temperature)
    {
      thermostat_component->maximum_temperature = thermostat_component->current_temperature;
      thermostat_component->send_maximum_temperature_property = true;
    }

    if (thermostat_component->current_temperature
        < thermostat_component->minimum_temperature)
    {
      thermostat_component->minimum_temperature = thermostat_component->current_temperature;
    }

    // Calculate and update the new average temperature.
    thermostat_component->temperature_count++;
    thermostat_component->temperature_summation
        += thermostat_component->current_temperature;
    thermostat_component->average_temperature = thermostat_component->temperature_summation
        / thermostat_component->temperature_count;

    IOT_SAMPLE_LOG_SUCCESS("Client updated desired temperature variables locally.");
    IOT_SAMPLE_LOG("Current Temperature: %2f", thermostat_component->current_temperature);
    IOT_SAMPLE_LOG("Maximum Temperature: %2f", thermostat_component->maximum_temperature);
    IOT_SAMPLE_LOG("Minimum Temperature: %2f", thermostat_component->minimum_temperature);
    IOT_SAMPLE_LOG("Average Temperature: %2f", thermostat_component->average_temperature);
}

// TODO: temperature => desiredTemperature or some such
static void pnp_thermostat_process_property_acknowledgement(az_iot_hub_client const* hub_client, pnp_thermostat_component* thermostat_component, double temperature, int32_t version,
    az_span payload,
    az_span* out_payload)
{
  const char* const property_log = "Failed to create property with status payload";

  az_json_writer jw;
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_init(&jw, payload, NULL), "Failed to initialize the json writer");

  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_begin_object(&jw), property_log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_begin_component(
          hub_client, &jw, thermostat_component->component_name),
      property_log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_begin_response_status(
          hub_client,
          &jw,
          property_desired_temperature_property_name,
          (int32_t)AZ_IOT_STATUS_OK,
          version,
          property_response_success),
      property_log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_json_writer_append_double(&jw, temperature, DOUBLE_DECIMAL_PLACE_DIGITS),
      property_log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_end_response_status(hub_client, &jw), property_log);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      az_iot_hub_client_properties_builder_end_component(hub_client, &jw), property_log);
  
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_writer_append_end_object(&jw), property_log);
  
  *out_payload = az_json_writer_get_bytes_used_in_destination(&jw);
}


// TODO: (a) be consistent on whether thermostat_component is 1st argument or not, (b) removely dup/not needed stuff from component's headers
az_result pnp_thermostat_process_property_update(
    az_iot_hub_client const* hub_client,
    pnp_thermostat_component* thermostat_component,
    az_json_reader* property_name_and_value,
    MQTTClient mqtt_client,
    int32_t version)
{
  char const* const log = "Failed to process property update";
  double temperature = 0;

  pnp_mqtt_message publish_message;
  pnp_mqtt_message_init(&publish_message);

  if (!az_json_token_is_text_equal(
          &property_name_and_value->token, property_desired_temperature_property_name))
  {
    // Ignore token, but we need to advance.  TODO: investigate how to do this if nested.
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(property_name_and_value), log);
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(property_name_and_value), log);
    return false;
  }
  else
  {
    IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(property_name_and_value), log);

    IOT_SAMPLE_EXIT_IF_AZ_FAILED(
        az_json_token_get_double(&property_name_and_value->token, &temperature), log);

    IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(property_name_and_value), log);

    pnp_thermostat_update_device_temperature_property(thermostat_component, temperature);

    pnp_thermostat_process_property_acknowledgement(hub_client, thermostat_component, temperature, version, publish_message.payload, &publish_message.out_payload);

    // Send response to the updated property.
    publish_mqtt_message(
        mqtt_client, publish_message.topic, publish_message.out_payload, IOT_SAMPLE_MQTT_PUBLISH_QOS);
    IOT_SAMPLE_LOG_SUCCESS("Client sent reported property message:");
    IOT_SAMPLE_LOG_AZ_SPAN("Payload:", publish_message.out_payload);   
  }

  if (thermostat_component->send_maximum_temperature_property == true)
  {
    pnp_thermostat_update_maximum_temperature_property(thermostat_component, hub_client, mqtt_client, &publish_message);
    thermostat_component->send_maximum_temperature_property = false;
  }

  return true;
}

bool pnp_thermostat_process_command_request(
    pnp_thermostat_component const* thermostat_component,
    az_iot_hub_client const* hub_client,
    MQTTClient mqtt_client,
    az_iot_hub_client_command_request const* command_request,
    az_span command_received_payload)
{
  pnp_mqtt_message publish_message;
  pnp_mqtt_message_init(&publish_message);

  az_iot_status status;

  if (az_span_is_content_equal(command_getMaxMinReport_name, command_request->command_name))
  {
    // Invoke command.
    if (invoke_getMaxMinReport(
            thermostat_component, command_received_payload, publish_message.payload, &publish_message.out_payload))
    {
      status = AZ_IOT_STATUS_OK;
    }
    else
    {
      publish_message.out_payload = command_empty_response_payload;
      status = AZ_IOT_STATUS_BAD_REQUEST;
      IOT_SAMPLE_LOG_AZ_SPAN(
          "Bad request when invoking command on Thermostat Sensor component:", command_request->command_name);
    }
  }
  else // Unsupported command
  {
    publish_message.out_payload = command_empty_response_payload;
    status = AZ_IOT_STATUS_NOT_FOUND;
    IOT_SAMPLE_LOG_AZ_SPAN("Command not supported on Thermostat Sensor component:", command_request->command_name);
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
