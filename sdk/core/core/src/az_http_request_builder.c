// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <az_http_request_builder.h>

az_http_request_defaults const az_http_request_builder_defaults
    = { .max_url_size = AZ_HTTP_REQUEST_DEFAULT_URL_SIZE,
        .max_headers = AZ_HTTP_REQUEST_DEFAULT_MAX_HEADERS,
        .min_buffer_size = AZ_HTTP_REQUEST_DEFAULT_URL_SIZE
            + (AZ_HTTP_REQUEST_DEFAULT_HEADER_SIZE * AZ_HTTP_REQUEST_DEFAULT_MAX_HEADERS)
            + AZ_HTTP_REQUEST_DEFAULT_MAX_BODY_SIZE };

az_result az_http_request_builder_init(
    az_http_request_builder * const out,
    az_span const buffer,
    az_const_span const method_verb) {
  if (buffer.size < (size_t)az_http_request_builder_defaults.min_buffer_size) {
    return AZ_ERROR_ARG;
  }
  (void)method_verb;
  (void)out;

  // default size for url. User can update it calling set_max_url_size after init
  az_http_request_builder_set_max_url_size(out, az_http_request_builder_defaults.max_url_size);

  return AZ_OK;
}

/*
az_result az_http_request_builder_set_initial_url(
    az_http_request_builder * const p_builder,
    az_const_span const url) {
  return AZ_OK;
}

az_result az_http_request_builder_set_query_parameter(
    az_http_request_builder * const p_builder,
    az_const_span const name,
    az_const_span const value) {
  return AZ_OK;
}

az_result az_http_request_builder_append_header(
    az_http_request_builder * const p_builder,
    az_const_span const name,
    az_const_span const value) {
  return AZ_OK;
}

az_result az_http_request_builder_mark_retry_headers_start(
    az_http_request_builder * const p_builder) {
  return AZ_OK;
}

az_result az_http_request_builder_remove_retry_headers(az_http_request_builder * const p_builder) {
  return AZ_OK;
}
*/
