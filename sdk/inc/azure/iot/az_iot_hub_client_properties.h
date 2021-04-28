// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/**
 * @file
 *
 * @brief Definition for the Azure IoT Plug and Play property builder and parsing routines.
 *
 * @warning THIS LIBRARY IS IN PREVIEW. APIS ARE SUBJECT TO CHANGE UNTIL GENERAL AVAILABILITY.
 */

#ifndef _az_IOT_HUB_CLIENT_PROPERTIES_H
#define _az_IOT_HUB_CLIENT_PROPERTIES_H

#include <stdbool.h>
#include <stdint.h>

#include <azure/core/az_json.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>

#include <azure/iot/az_iot_hub_client.h>

#include <azure/core/_az_cfg_prefix.h>

/**
 * @brief Append the necessary characters to a reported property JSON payload belonging to a
 * component.
 *
 * The payload will be of the form:
 *
 * @code
 * "reported": {
 *     "<component_name>": {
 *         "__t": "c",
 *         "temperature": 23
 *     }
 * }
 * @endcode
 *
 * @note This API should be used in conjunction with
 * az_iot_hub_client_property_builder_end_component().
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_writer The #az_json_writer to append the necessary characters for an IoT
 * Plug and Play component.
 * @param[in] component_name The component name associated with the reported property.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_writer must not be `NULL`.
 * @pre \p component_name must be a valid, non-empty #az_span.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK The JSON payload was prefixed successfully.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_builder_begin_component(
    az_iot_hub_client const* client,
    az_json_writer* ref_json_writer,
    az_span component_name);

/**
 * @brief Append the necessary characters to end a reported property JSON payload belonging to a
 * component.
 *
 * @note This API should be used in conjunction with
 * az_iot_hub_client_property_builder_begin_component().
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_writer The #az_json_writer to append the necessary characters for an IoT
 * Plug and Play component.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_writer must not be `NULL`.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK The JSON payload was suffixed successfully.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_builder_end_component(
    az_iot_hub_client const* client,
    az_json_writer* ref_json_writer);

/**
 * @brief Begin a property response payload with confirmation status.
 *
 * This API should be used in response to an incoming writeable property. More details can be found
 * here:
 *
 * https://docs.microsoft.com/en-us/azure/iot-pnp/concepts-convention#writable-properties
 *
 * The payload will be of the form:
 *
 * **Without component**
 * @code
 * //{
 * //  "<property_name>":{
 * //    "ac": <ack_code>,
 * //    "av": <ack_version>,
 * //    "ad": "<ack_description>",
 * //    "value": <user_value>
 * //  }
 * //}
 * @endcode
 *
 *
 * To send a status for a property belonging to a component, first call the
 * az_iot_hub_client_property_builder_begin_component() API to prefix the payload with the
 * necessary identification. The API call flow would look like the following with the listed JSON
 * payload being generated.
 *
 * **With component**
 * @code
 *
 * az_iot_hub_client_property_builder_begin_component()
 * az_iot_hub_client_property_builder_begin_response_status()
 * // Append user value here (<user_value>) using ref_json_writer directly.
 * az_iot_hub_client_property_builder_end_response_status()
 * az_iot_hub_client_property_builder_end_component()
 *
 * //{
 * //  "<component_name>": {
 * //    "__t": "c",
 * //    "<property_name>": {
 * //      "ac": <ack_code>,
 * //      "av": <ack_version>,
 * //      "ad": "<ack_description>",
 * //      "value": <user_value>
 * //    }
 * //  }
 * //}
 * @endcode
 *
 * @note This API should be used in conjunction with
 * az_iot_hub_client_property_builder_end_response_status().
 *
 * @note This API only builds the metadata for the property response.  The
 * application itself must specify the payload contents (// Append user value
 * here, from above).
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_writer The initialized #az_json_writer to append data to.
 * @param[in] property_name The name of the property to build a response payload for.
 * @param[in] ack_code The HTTP-like status code to respond with. See #az_iot_status for
 * possible supported values.
 * @param[in] ack_version The version of the property the application is acknowledging.
 * This can be retrieved from the service request by
 * calling az_iot_hub_client_property_get_property_version.
 * @param[in] ack_description An optional description detailing the context or any details about
 * the acknowledgement. This can be #AZ_SPAN_EMPTY.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_writer must not be `NULL`.
 * @pre \p property_name must be a valid, non-empty #az_span.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK The JSON payload was prefixed successfully.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_builder_begin_response_status(
    az_iot_hub_client const* client,
    az_json_writer* ref_json_writer,
    az_span property_name,
    int32_t ack_code,
    int32_t ack_version,
    az_span ack_description);

/**
 * @brief End a property response payload with confirmation status.
 *
 * @note This API should be used in conjunction with
 * az_iot_hub_client_property_builder_begin_response_status().
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_writer The initialized #az_json_writer to append data to.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_writer must not be `NULL`.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK The JSON payload was suffixed successfully.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_builder_end_response_status(
    az_iot_hub_client const* client,
    az_json_writer* ref_json_writer);

/**
 * @brief Read the IoT Plug and Play property version.
 *
 * @warning This modifies the state of the json reader. To then use the same json reader
 * with az_iot_hub_client_property_get_next_component_property(), you must call
 * az_json_reader_init() again after this call and before the call to
 * az_iot_hub_client_property_get_next_component_property() or make an additional copy before
 * calling this API.
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_reader The pointer to the #az_json_reader used to parse through the JSON
 * payload.
 * @param[in] response_type The #az_iot_hub_client_property_response_type representing the message
 * type associated with the payload.
 * @param[out] out_version The numeric version of the properties in the JSON payload.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_reader must not be `NULL`.
 * @pre \p out_version must not be `NULL`.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK If the function returned a valid version.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_get_property_version(
    az_iot_hub_client const* client,
    az_json_reader* ref_json_reader,
    az_iot_hub_client_property_response_type response_type,
    int32_t* out_version);

/**
 * @brief Property type
 *
 */
typedef enum
{
  AZ_IOT_HUB_CLIENT_PROPERTY_REPORTED_FROM_DEVICE = 1,
  AZ_IOT_HUB_CLIENT_PROPERTY_WRITEABLE = 2
} az_iot_hub_client_property_type;

/**
 * @brief Iteratively read the IoT Plug and Play component properties.
 *
 * Note that between calls, the #az_span pointed to by \p out_component_name shall not be modified,
 * only checked and compared. Internally, the #az_span is only changed if the component name changes
 * in the JSON document and is not necessarily set every invocation of the function.
 *
 * On success, the `ref_json_reader` will be set on a valid property name. After checking the
 * property name, the reader can be advanced to the property value by calling
 * az_json_reader_next_token(). Note that on the subsequent call to this API, it is expected that
 * the json reader will be placed AFTER the read property name and value. That means that after
 * reading the property value (including single values or complex objects), the user must call
 * az_json_reader_next_token().
 *
 * Below is a code snippet which you can use as a starting point:
 *
 * @code
 *
 * while (az_result_succeeded(az_iot_hub_client_property_get_next_component_property(
 *       &hub_client, &jr, response_type, property_type, &component_name)))
 * {
 *   // Check if property is of interest (substitute user_property for your own property name)
 *   if (az_json_token_is_text_equal(&jr.token, user_property))
 *   {
 *     az_json_reader_next_token(&jr);
 *
 *     // Get the property value here
 *     // Example: az_json_token_get_int32(&jr.token, &user_int);
 *
 *     // Skip to next property value
 *     az_json_reader_next_token(&jr);
 *   }
 *   else
 *   {
 *     // The JSON reader must be advanced regardless of whether the property
 *     // is of interest or not.
 *     az_json_reader_next_token(&jr);
 *
 *     // Skip children in case the property value is an object
 *     az_json_reader_skip_children(&jr);
 *     az_json_reader_next_token(&jr);
 *   }
 * }
 *
 * @endcode
 *
 * @param[in] client The #az_iot_hub_client to use for this call.
 * @param[in,out] ref_json_reader The #az_json_reader to parse through. The ownership of iterating
 * through this json reader is shared between the user and this API.
 * @param[in] response_type The #az_iot_hub_client_property_response_type representing the message
 * type associated with the payload.
 * @param[out] out_component_name The #az_span* representing the value of the component.
 *
 * @pre \p client must not be `NULL`.
 * @pre \p ref_json_reader must not be `NULL`.
 * @pre \p out_component_name must not be `NULL`. It must point to an #az_span instance.
 *
 * @return An #az_result value indicating the result of the operation.
 * @retval #AZ_OK If the function returned a valid #az_json_reader pointing to the property name and
 * the #az_span with a component name.
 * @retval #AZ_ERROR_JSON_INVALID_STATE If the json reader is passed in at an unexpected location.
 * @retval #AZ_ERROR_IOT_END_OF_PROPERTIES If there are no more properties left for the component.
 */
AZ_NODISCARD az_result az_iot_hub_client_property_get_next_component_property(
    az_iot_hub_client const* client,
    az_json_reader* ref_json_reader,
    az_iot_hub_client_property_response_type response_type,
    az_span* out_component_name,
    az_iot_hub_client_property_type* property_type);

#include <azure/core/_az_cfg_suffix.h>

#endif //_az_IOT_HUB_CLIENT_PROPERTIES_H
