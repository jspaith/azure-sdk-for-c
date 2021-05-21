// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "iot_sample_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <azure/core/az_json.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>
#include <azure/iot/az_iot_hub_client_properties.h>

#include "iot_sample_common.h"
#include "pnp/pnp_device_info_component.h"
#include "pnp/pnp_mqtt_message.h"
#include "pnp/pnp_thermostat_component.h"
#include "pnp/pnp_tempcontroller_component.h"

#define SAMPLE_TYPE PAHO_IOT_HUB
#define SAMPLE_NAME PAHO_IOT_PNP_COMPONENT_SAMPLE

#define DEFAULT_START_TEMP_CELSIUS 22.0
#define DOUBLE_DECIMAL_PLACE_DIGITS 2

bool is_device_operational = true;

// * PnP Values *
// The model id is the JSON document (also called the Digital Twins Model Identifier or DTMI) which
// defines the capability of your device. The functionality of the device should match what is
// described in the corresponding DTMI. Should you choose to program your own PnP capable device,
// the functionality would need to match the DTMI and you would need to update the below 'model_id'.
// Please see the sample README for more information on this DTMI.
static az_span const model_id
    = AZ_SPAN_LITERAL_FROM_STR("dtmi:com:example:TemperatureController;1");

// Components
static pnp_thermostat_component thermostat_1;
static pnp_thermostat_component thermostat_2;
static az_span thermostat_1_name = AZ_SPAN_LITERAL_FROM_STR("thermostat1");
static az_span thermostat_2_name = AZ_SPAN_LITERAL_FROM_STR("thermostat2");
static az_span pnp_device_components[] = { AZ_SPAN_LITERAL_FROM_STR("thermostat1"),
                                           AZ_SPAN_LITERAL_FROM_STR("thermostat2"),
                                           AZ_SPAN_LITERAL_FROM_STR("deviceInformation") };
static int32_t const pnp_components_length
    = sizeof(pnp_device_components) / sizeof(pnp_device_components[0]);

// Plug and Play command values
static az_span const command_empty_response_payload = AZ_SPAN_LITERAL_FROM_STR("{}");

static iot_sample_environment_variables env_vars;
static az_iot_hub_client hub_client;
static MQTTClient mqtt_client;
static char mqtt_client_username_buffer[512];
static pnp_mqtt_message publish_message;

//
// Functions
//
static void create_and_configure_mqtt_client(void);
static void connect_mqtt_client_to_iot_hub(void);
static void subscribe_mqtt_client_to_iot_hub_topics(void);
static void initialize_components(void);
static void send_device_info(void);
static void send_serial_number(void);
static void request_all_properties(void);
static void receive_messages(void);
static void disconnect_mqtt_client_from_iot_hub(void);

// General message sending and receiving functions
static void receive_mqtt_message(void);
static void on_message_received(
    char* topic,
    int topic_len,
    MQTTClient_message const* receive_message);

// Device property, command request, telemetry functions
static void handle_device_property_message(
    MQTTClient_message const* receive_message,
    az_iot_hub_client_properties_response const* property_response);
static void handle_command_request(
    MQTTClient_message const* receive_message,
    az_iot_hub_client_command_request const* command_request);
static void send_telemetry_messages(void);

// Temperature controller functions
static void temp_controller_build_telemetry_message(az_span payload, az_span* out_payload);
static void temp_controller_build_serial_number_reported_property(
    az_span payload,
    az_span* out_payload);
static void temp_controller_invoke_reboot(void);

static az_result append_simple_json_token(az_json_writer* jw, az_json_token* json_token);

/*
 * This sample extends the Azure IoT Plug and Play Sample above to mimic a Temperature Controller
 * and connects the Plug and Play enabled device (the Temperature Controller) with the Digital
 * Twin Model ID (DTMI). If a timeout occurs while waiting for a message from the Azure IoT
 * Explorer, the sample will continue. If PNP_MQTT_TIMEOUT_RECEIVE_MAX_COUNT timeouts occur
 * consecutively, the sample will disconnect. X509 self-certification is used.
 *
 * This Temperature Controller is made up of the following components:
 * - Device Info
 * - Temperature Sensor 1
 * - Temperature Sensor 2
 *
 * To interact with this sample, you must use the Azure IoT Explorer. The capabilities are
 * properties, commands, and telemetry:
 */
int main(void)
{
  create_and_configure_mqtt_client();
  IOT_SAMPLE_LOG_SUCCESS("Client created and configured.");

  connect_mqtt_client_to_iot_hub();
  IOT_SAMPLE_LOG_SUCCESS("Client connected to IoT Hub.");

  subscribe_mqtt_client_to_iot_hub_topics();
  IOT_SAMPLE_LOG_SUCCESS("Client subscribed to IoT Hub topics.");

  // Initializations
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(
      pnp_mqtt_message_init(&publish_message), "Failed to initialize pnp_mqtt_message");

  initialize_components();
  IOT_SAMPLE_LOG_SUCCESS("Client initialized all components.");

  // Messaging
  send_device_info();
  send_serial_number();
  request_all_properties();
  receive_messages();

  disconnect_mqtt_client_from_iot_hub();
  IOT_SAMPLE_LOG_SUCCESS("Client disconnected from IoT Hub.");

  return 0;
}

static void create_and_configure_mqtt_client(void)
{
  // Reads in environment variables set by user for purposes of running sample.
  iot_sample_read_environment_variables(SAMPLE_TYPE, SAMPLE_NAME, &env_vars);

  // Build an MQTT endpoint c-string.
  char mqtt_endpoint_buffer[128];
  iot_sample_create_mqtt_endpoint(
      SAMPLE_TYPE, &env_vars, mqtt_endpoint_buffer, sizeof(mqtt_endpoint_buffer));

  // Initialize the pnp client with the connection options.
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.model_id = model_id;
  options.component_names = pnp_device_components;
  options.component_names_length = pnp_components_length;

  int rc = az_iot_hub_client_init(
      &hub_client, env_vars.hub_hostname, env_vars.hub_device_id, &options);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to initialize pnp client");

  // Get the MQTT client id used for the MQTT connection.
  char mqtt_client_id_buffer[128];

  rc = az_iot_hub_client_get_client_id(
      &hub_client, mqtt_client_id_buffer, sizeof(mqtt_client_id_buffer), NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get MQTT client id");

  // Create the Paho MQTT client.
  rc = MQTTClient_create(
      &mqtt_client, mqtt_endpoint_buffer, mqtt_client_id_buffer, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR("Failed to create MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }
}

static void connect_mqtt_client_to_iot_hub(void)
{
  // Get the MQTT client username.
  az_result rc = az_iot_hub_client_get_user_name(
      &hub_client, mqtt_client_username_buffer, sizeof(mqtt_client_username_buffer), NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get MQTT client username");

  // Set MQTT connection options.
  MQTTClient_connectOptions mqtt_connect_options = MQTTClient_connectOptions_initializer;
  mqtt_connect_options.username = mqtt_client_username_buffer;
  mqtt_connect_options.password = NULL; // This sample uses x509 authentication.
  mqtt_connect_options.cleansession = false; // Set to false so can receive any pending messages.
  mqtt_connect_options.keepAliveInterval = AZ_IOT_DEFAULT_MQTT_CONNECT_KEEPALIVE_SECONDS;

  MQTTClient_SSLOptions mqtt_ssl_options = MQTTClient_SSLOptions_initializer;
  mqtt_ssl_options.verify = 1;
  mqtt_ssl_options.enableServerCertAuth = 1;
  mqtt_ssl_options.keyStore = (char*)az_span_ptr(env_vars.x509_cert_pem_file_path);
  if (az_span_size(env_vars.x509_trust_pem_file_path) != 0) // Is only set if required by OS.
  {
    mqtt_ssl_options.trustStore = (char*)az_span_ptr(env_vars.x509_trust_pem_file_path);
  }
  mqtt_connect_options.ssl = &mqtt_ssl_options;

  // Connect MQTT client to the Azure IoT Hub.
  rc = MQTTClient_connect(mqtt_client, &mqtt_connect_options);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR(
        "Failed to connect: MQTTClient return code %d.\n"
        "If on Windows, confirm the AZ_IOT_DEVICE_X509_TRUST_PEM_FILE_PATH environment variable is "
        "set correctly.",
        rc);
    exit(rc);
  }
}

static void subscribe_mqtt_client_to_iot_hub_topics(void)
{
  // Messages received on the command topic will be commands to be invoked.
  int rc = MQTTClient_subscribe(
      mqtt_client, AZ_IOT_HUB_CLIENT_COMMANDS_SUBSCRIBE_TOPIC, IOT_SAMPLE_MQTT_SUBSCRIBE_QOS);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR(
        "Failed to subscribe to the command topic: MQTTClient return code %d.", rc);
    exit(rc);
  }

  // Messages received on the property PATCH topic will be updates to the desired properties.
  rc = MQTTClient_subscribe(
      mqtt_client,
      AZ_IOT_HUB_CLIENT_PROPERTIES_PATCH_SUBSCRIBE_TOPIC,
      IOT_SAMPLE_MQTT_SUBSCRIBE_QOS);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR(
        "Failed to subscribe to the property PATCH topic: MQTTClient return code %d.", rc);
    exit(rc);
  }

  // Messages received on property response topic will be response statuses from the server.
  rc = MQTTClient_subscribe(
      mqtt_client,
      AZ_IOT_HUB_CLIENT_PROPERTIES_RESPONSE_SUBSCRIBE_TOPIC,
      IOT_SAMPLE_MQTT_SUBSCRIBE_QOS);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR(
        "Failed to subscribe to the property response topic: MQTTClient return code %d.", rc);
    exit(rc);
  }
}

static void initialize_components(void)
{
  // Initialize thermostats 1 and 2.
  az_result rc = pnp_thermostat_init(&thermostat_1, thermostat_1_name, DEFAULT_START_TEMP_CELSIUS);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to initialize Temperature Sensor 1");

  rc = pnp_thermostat_init(&thermostat_2, thermostat_2_name, DEFAULT_START_TEMP_CELSIUS);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to initialize Temperature Sensor 2");
}

static void send_device_info(void)
{
  // Build and send the device info reported property message.
  pnp_device_info_send_reported_properties(&hub_client, mqtt_client);
}

static void send_serial_number(void)
{
    pnp_temp_controller_send_serial_number(&hub_client, mqtt_client);
}

static void request_all_properties(void)
{
  IOT_SAMPLE_LOG("Client requesting device property document from service.");

  // Get the property document topic to publish the property document request.
  az_result rc = az_iot_hub_client_properties_document_get_publish_topic(
      &hub_client,
      pnp_mqtt_get_request_id(),
      publish_message.topic,
      publish_message.topic_length,
      NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get the property document topic");

  // Publish the property document request.
  publish_mqtt_message(mqtt_client, publish_message.topic, AZ_SPAN_EMPTY, IOT_SAMPLE_MQTT_PUBLISH_QOS);
  IOT_SAMPLE_LOG(" "); // Formatting
}

static void receive_messages(void)
{
  // Continue to receive commands or device property messages while device is operational.
  while (is_device_operational)
  {
    // Send telemetry messages. No response requested from server.
    send_telemetry_messages();
    IOT_SAMPLE_LOG(" "); // Formatting.

    // Wait for any server-initiated messages.
    receive_mqtt_message();
  }
}

static void disconnect_mqtt_client_from_iot_hub(void)
{
  int rc = MQTTClient_disconnect(mqtt_client, PNP_MQTT_TIMEOUT_DISCONNECT_MS);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR("Failed to disconnect MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }

  MQTTClient_destroy(&mqtt_client);
}

static void receive_mqtt_message(void)
{
  char* topic = NULL;
  int topic_len = 0;
  MQTTClient_message* receive_message = NULL;
  static int8_t timeout_counter;

  IOT_SAMPLE_LOG("Waiting for command request or device property message.\n");

  // MQTTCLIENT_SUCCESS or MQTTCLIENT_TOPICNAME_TRUNCATED if a message is received.
  // MQTTCLIENT_SUCCESS can also indicate that the timeout expired, in which case message is NULL.
  // MQTTCLIENT_TOPICNAME_TRUNCATED if the topic contains embedded NULL characters.
  // An error code is returned if there was a problem trying to receive a message.
  int rc = MQTTClient_receive(
      mqtt_client, &topic, &topic_len, &receive_message, PNP_MQTT_TIMEOUT_RECEIVE_MS);
  if ((rc != MQTTCLIENT_SUCCESS) && (rc != MQTTCLIENT_TOPICNAME_TRUNCATED))
  {
    IOT_SAMPLE_LOG_ERROR("Failed to receive message: MQTTClient return code %d.", rc);
    exit(rc);
  }
  else if (receive_message == NULL)
  {
    // Allow up to TIMEOUT_MQTT_RECEIVE_MAX_COUNT before disconnecting.
    if (++timeout_counter >= PNP_MQTT_TIMEOUT_RECEIVE_MAX_MESSAGE_COUNT)
    {
      IOT_SAMPLE_LOG(
          "Receive message timeout expiration count of %d reached.",
          PNP_MQTT_TIMEOUT_RECEIVE_MAX_MESSAGE_COUNT);
      is_device_operational = false;
    }
  }
  else
  {
    IOT_SAMPLE_LOG_SUCCESS("Client received a message from the service.");
    timeout_counter = 0; // Reset

    if (rc == MQTTCLIENT_TOPICNAME_TRUNCATED)
    {
      topic_len = (int)strlen(topic);
    }

    on_message_received(topic, topic_len, receive_message);
    IOT_SAMPLE_LOG(" "); // Formatting

    MQTTClient_freeMessage(&receive_message);
    MQTTClient_free(topic);
  }
}

static void on_message_received(
    char* topic,
    int topic_len,
    MQTTClient_message const* receive_message)
{
  az_result rc;

  az_span const topic_span = az_span_create((uint8_t*)topic, topic_len);
  az_span const message_span
      = az_span_create((uint8_t*)receive_message->payload, receive_message->payloadlen);

  az_iot_hub_client_properties_response property_response;
  az_iot_hub_client_command_request command_request;

  // Parse the incoming message topic and handle appropriately.
  rc = az_iot_hub_client_properties_parse_received_topic(
      &hub_client, topic_span, &property_response);
  if (az_result_succeeded(rc))
  {
    IOT_SAMPLE_LOG_SUCCESS("Client received a valid property topic response.");
    IOT_SAMPLE_LOG_AZ_SPAN("Topic:", topic_span);
    IOT_SAMPLE_LOG_AZ_SPAN("Payload:", message_span);
    IOT_SAMPLE_LOG("Status: %d", property_response.status);

    handle_device_property_message(receive_message, &property_response);
  }
  else
  {
    rc = az_iot_hub_client_commands_parse_received_topic(&hub_client, topic_span, &command_request);
    if (az_result_succeeded(rc))
    {
      IOT_SAMPLE_LOG_SUCCESS("Client received a valid command topic response.");
      IOT_SAMPLE_LOG_AZ_SPAN("Topic:", topic_span);
      IOT_SAMPLE_LOG_AZ_SPAN("Payload:", message_span);

      handle_command_request(receive_message, &command_request);
    }
    else
    {
      IOT_SAMPLE_LOG_ERROR("Message from unknown topic: az_result return code 0x%08x.", rc);
      IOT_SAMPLE_LOG_AZ_SPAN("Topic:", topic_span);
      exit(rc);
    }
  }
}

static void process_property_message(
    az_span property_message_span,
    az_iot_hub_client_properties_response_type response_type)
{
  az_result rc = az_iot_hub_client_properties_patch_get_publish_topic(
      &hub_client,
      pnp_mqtt_get_request_id(),
      publish_message.topic,
      publish_message.topic_length,
      NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Failed to get the property PATCH topic");

  az_json_reader jr;
  az_span component_name;
  int32_t version = 0;
  rc = az_json_reader_init(&jr, property_message_span, NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Could not initialize the json reader");

  rc = az_iot_hub_client_properties_get_properties_version(
      &hub_client, &jr, response_type, &version);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Could not get the property version");

  rc = az_json_reader_init(&jr, property_message_span, NULL);
  IOT_SAMPLE_EXIT_IF_AZ_FAILED(rc, "Could not initialize the json reader");

  while (az_result_succeeded(
      rc = az_iot_hub_client_properties_get_next_component_property(
          &hub_client, &jr, response_type, AZ_IOT_HUB_CLIENT_PROPERTY_WRITEABLE, &component_name)))
  {
    if (rc == AZ_OK)
    {
      if (az_span_is_content_equal(component_name, thermostat_1_name))
      {
        rc = pnp_thermostat_process_property_update(
            &hub_client,
            &thermostat_1,
            &jr,
            mqtt_client,
            version);
      }
      else if (az_span_is_content_equal(component_name, thermostat_2_name))
      {
        rc = pnp_thermostat_process_property_update(
            &hub_client,
            &thermostat_2,
            &jr,
            mqtt_client,
            version);
      }
      else
      {
        char const* const log = "Failed to process property update";

        // Only the thermostat component supports writeable properties;
        // the models for DeviceInfo and the temperature controller do not.
        // We do NOT report back an error for an unknown property to IoT Hub, 
        // but we do need to skip past the JSON part of the body to continue reading.
        IOT_SAMPLE_LOG_ERROR("Received property update for an unsupported component");

        IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(&jr), log);
        IOT_SAMPLE_EXIT_IF_AZ_FAILED(az_json_reader_next_token(&jr), log);
      }
    }
    else
    {
      IOT_SAMPLE_LOG_ERROR("Failed to update a property: az_result return code 0x%08x.", rc);
      exit(rc);
    }
  }
}

static void handle_device_property_message(
    MQTTClient_message const* receive_message,
    az_iot_hub_client_properties_response const* property_response)
{
  az_span const message_span
      = az_span_create((uint8_t*)receive_message->payload, receive_message->payloadlen);

  // Invoke appropriate action per response type (3 types only).
  switch (property_response->response_type)
  {
    // A response from a property GET publish message with the property document as a payload.
    case AZ_IOT_HUB_CLIENT_PROPERTIES_RESPONSE_TYPE_GET:
      IOT_SAMPLE_LOG("Message Type: GET");
      (void)process_property_message(message_span, property_response->response_type);
      break;

    // An update to the desired properties with the properties as a payload.
    case AZ_IOT_HUB_CLIENT_PROPERTIES_RESPONSE_TYPE_DESIRED_PROPERTIES:
      IOT_SAMPLE_LOG("Message Type: Desired Properties");
      (void)process_property_message(message_span, property_response->response_type);

      break;

    // A response from a property reported properties publish message.
    case AZ_IOT_HUB_CLIENT_PROPERTIES_RESPONSE_TYPE_REPORTED_PROPERTIES:
      IOT_SAMPLE_LOG("Message Type: Reported Properties");
      break;
  }
}

static void handle_command_request(
    MQTTClient_message const* receive_message,
    az_iot_hub_client_command_request const* command_request)
{
  az_span const message_span
      = az_span_create((uint8_t*)receive_message->payload, receive_message->payloadlen);
  az_iot_status status = AZ_IOT_STATUS_UNKNOWN;

  // Invoke command and retrieve status and response payload to send to server.
  if (az_span_size(command_request->component_name) == 0)
  {
    if (pnp_temp_controller_process_command_request(&hub_client, mqtt_client, command_request, message_span))
    {
      IOT_SAMPLE_LOG_AZ_SPAN(
          "Client invoked command on Temperature Controller:", command_request->command_name);
    }
  }
  else if (az_span_is_content_equal(thermostat_1.component_name, command_request->component_name))
  {
    if (pnp_thermostat_process_command_request(
            &thermostat_1,
            &hub_client,
            mqtt_client,
            command_request,
            message_span))
    {
      IOT_SAMPLE_LOG_AZ_SPAN(
          "Client invoked command on Temperature Sensor 1:", command_request->command_name);
    }
  }
  else if (az_span_is_content_equal(thermostat_2.component_name, command_request->component_name))
  {
    if (pnp_thermostat_process_command_request(
            &thermostat_2,
            &hub_client,
            mqtt_client,
            command_request,
            message_span))
    {
      IOT_SAMPLE_LOG_AZ_SPAN(
          "Client invoked command on Temperature Sensor 1:", command_request->command_name);
    }
  }
  else
  {
    IOT_SAMPLE_LOG_AZ_SPAN("Command not supported:", command_request->command_name);
    publish_message.out_payload = command_empty_response_payload;
    status = AZ_IOT_STATUS_NOT_FOUND;

    // Get the commands response topic to publish the command response.
    az_result rc = az_iot_hub_client_commands_response_get_publish_topic(
        &hub_client,
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

  }
}

static void send_telemetry_messages(void)
{
  pnp_thermostat_send_telemetry_message(&thermostat_1, &hub_client, mqtt_client);
  pnp_thermostat_send_telemetry_message(&thermostat_2, &hub_client, mqtt_client);
  pnp_temp_controller_send_telemetry_message(&hub_client, mqtt_client);
}

