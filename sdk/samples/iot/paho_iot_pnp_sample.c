// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/*
 * This sample connects an Azure IoT Plug and Play enabled device with the Digital Twin Model ID
 * (DTMI). If a timeout occurs while waiting for a message from the Azure IoT Explorer, the sample
 * will continue. If MQTT_TIMEOUT_RECEIVE_MAX_MESSAGE_COUNT timeouts occur consecutively, the sample
 * will disconnect.
 *
 * An X509 self-certification is used for authentication.
 *
 */

#ifdef _MSC_VER
#pragma warning(push)
// warning C4201: nonstandard extension used: nameless struct/union
#pragma warning(disable : 4201)
#endif
#include <paho-mqtt/MQTTClient.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <azure/az_core.h>
#include <azure/az_iot.h>

#include "iot_sample_common.h"
#include "paho_iot_pnp_sample_common.h"

#include <azure/core/az_json.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>
#include <azure/iot/az_iot_hub_client_properties.h>

#define SAMPLE_TYPE PAHO_IOT_HUB
#define SAMPLE_NAME PAHO_IOT_PNP_SAMPLE

// * Plug and Play Values *
// The model ID is the JSON document (also called the Digital Twins Model Identifier or DTMI) which
// defines the capability of your device. The functionality of the device should match what is
// described in the corresponding DTMI. Should you choose to program your own PnP capable device,
// the functionality would need to match the DTMI and you would need to update the below 'model_id'.
// Please see the sample README for more information on this DTMI.
static az_span const model_id = AZ_SPAN_LITERAL_FROM_STR("dtmi:com:example:Thermostat;1");

static iot_sample_environment_variables env_vars;
static char mqtt_client_username_buffer[256];

//
// Functions
//
static void create_and_configure_mqtt_client(void);
static void connect_mqtt_client_to_iot_hub(void);
static void disconnect_mqtt_client_from_iot_hub(void);

int main(void)
{
  create_and_configure_mqtt_client();
  IOT_SAMPLE_LOG_SUCCESS("Client created and configured.");

  connect_mqtt_client_to_iot_hub();
  IOT_SAMPLE_LOG_SUCCESS("Client connected to IoT Hub.");

  paho_iot_pnp_sample_device_implement();
  IOT_SAMPLE_LOG_SUCCESS("Completed sample device implementation run.");

  disconnect_mqtt_client_from_iot_hub();
  IOT_SAMPLE_LOG_SUCCESS("Client disconnected from IoT Hub.");

  return 0;
}

// create_and_configure_mqtt_client reads configuration variables from the environment,
// makes calls to the Azure SDK for C for initial setup, and initiates an MQTT client
// from Paho.
static void create_and_configure_mqtt_client(void)
{
  int rc;

  // Reads in environment variables set by user for purposes of running sample.
  iot_sample_read_environment_variables(SAMPLE_TYPE, SAMPLE_NAME, &env_vars);

  // Build an MQTT endpoint c-string.
  char mqtt_endpoint_buffer[128];
  iot_sample_create_mqtt_endpoint(
      SAMPLE_TYPE, env_vars.hub_hostname, mqtt_endpoint_buffer, sizeof(mqtt_endpoint_buffer));

  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.model_id = model_id;

  rc = az_iot_hub_client_init(&hub_client, env_vars.hub_hostname, env_vars.hub_device_id, &options);
  if (az_result_failed(rc))
  {
    IOT_SAMPLE_LOG_ERROR("Failed to initialize hub client: az_result return code 0x%08x.", rc);
    exit(rc);
  }

  // Get the MQTT client id used for the MQTT connection.
  char mqtt_client_id_buffer[128];
  rc = az_iot_hub_client_get_client_id(
      &hub_client, mqtt_client_id_buffer, sizeof(mqtt_client_id_buffer), NULL);
  if (az_result_failed(rc))
  {
    IOT_SAMPLE_LOG_ERROR("Failed to get MQTT client id: az_result return code 0x%08x.", rc);
    exit(rc);
  }

  // Create the Paho MQTT client.
  rc = MQTTClient_create(
      &mqtt_client, mqtt_endpoint_buffer, mqtt_client_id_buffer, MQTTCLIENT_PERSISTENCE_NONE, NULL);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR("Failed to create MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }
}

// connect_mqtt_client_to_iot_hub sets up basic security and MQTT topics and then
// initiates an MQTT connection
static void connect_mqtt_client_to_iot_hub(void)
{
  int rc;

  // Get the MQTT client username.
  rc = az_iot_hub_client_get_user_name(
      &hub_client, mqtt_client_username_buffer, sizeof(mqtt_client_username_buffer), NULL);
  if (az_result_failed(rc))
  {
    IOT_SAMPLE_LOG_ERROR("Failed to get MQTT client username: az_result return code 0x%08x.", rc);
    exit(rc);
  }

  IOT_SAMPLE_LOG("MQTT client username: %s\n", mqtt_client_username_buffer);

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

  // Connect MQTT client to the Azure IoT Hub.  This will block until the connection
  // is established or fails.
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

// disconnect_mqtt_client_from_iot_hub disconnects and destroys the underlying MQTT connection and
// Paho handle.
static void disconnect_mqtt_client_from_iot_hub(void)
{
  int rc = MQTTClient_disconnect(mqtt_client, MQTT_TIMEOUT_DISCONNECT_MS);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    IOT_SAMPLE_LOG_ERROR("Failed to disconnect MQTT client: MQTTClient return code %d.", rc);
    exit(rc);
  }

  MQTTClient_destroy(&mqtt_client);
}

