#include "esp_http_api.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "desk_app.h"
#include "desk_build_secrets.h"
#include "desk_control_protocol.h"
#include "desk_form_codec.h"
#include "desk_http_security.h"
#include "desk_network_config.h"
#include "esp_app_desc.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_ota_update.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DESK_HTTP_BODY_MAX_SIZE 256U
#define DESK_HTTP_HEADER_MAX_SIZE 256U
#define DESK_HTTP_RECEIVE_TIMEOUT_LIMIT 3U
#define DESK_HTTP_WEBSOCKET_PROTOCOL "desk-control-v1"
#define DESK_HTTP_OTA_CHUNK_SIZE 1024U
#define DESK_HTTP_INHIBIT_TIMEOUT_MS 500U
#define DESK_HTTP_INHIBIT_POLL_MS 10U

static const char *const TAG = "desk_http";
static const char *const SESSION_COOKIE = "desk_session";

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");
extern const unsigned char app_css_start[] asm("_binary_app_css_start");
extern const unsigned char app_css_end[] asm("_binary_app_css_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[] asm("_binary_app_js_end");

static uint64_t now_ms(void)
{
    int64_t microseconds = esp_timer_get_time();
    return microseconds <= 0 ? 0U : (uint64_t)microseconds / 1000U;
}

static esp_err_t set_security_headers(httpd_req_t *request)
{
    esp_err_t result = httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "Content-Security-Policy",
                                    "default-src 'self'; connect-src 'self' wss:; img-src 'self'; "
                                    "style-src 'self'; script-src 'self'; object-src 'none'; "
                                    "base-uri 'none'; form-action 'self'; frame-ancestors 'none'");
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "Permissions-Policy",
                                    "camera=(), microphone=(), geolocation=()");
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_hdr(request, "Cross-Origin-Opener-Policy", "same-origin");
    }
    return result;
}

static esp_err_t send_json(httpd_req_t *request, const char *status, const char *json)
{
    esp_err_t result = set_security_headers(request);
    if (result == ESP_OK) {
        result = httpd_resp_set_status(request, status);
    }
    if (result == ESP_OK) {
        result = httpd_resp_set_type(request, HTTPD_TYPE_JSON);
    }
    return result == ESP_OK ? httpd_resp_sendstr(request, json) : result;
}

static esp_err_t send_error(httpd_req_t *request, const char *status, const char *code)
{
    char response[96];
    int written = snprintf(response, sizeof(response), "{\"error\":\"%s\"}", code);
    if (written <= 0 || (size_t)written >= sizeof(response)) {
        return ESP_FAIL;
    }
    return send_json(request, status, response);
}

static esp_err_t send_asset(httpd_req_t *request, const char *content_type,
                            const unsigned char *start, const unsigned char *end)
{
    if (start == NULL || end == NULL || end < start) {
        return ESP_FAIL;
    }
    esp_err_t result = set_security_headers(request);
    if (result == ESP_OK) {
        result = httpd_resp_set_type(request, content_type);
    }
    return result == ESP_OK ? httpd_resp_send(request, (const char *)start, (ssize_t)(end - start))
                            : result;
}

static esp_err_t index_handler(httpd_req_t *request)
{
    return send_asset(request, "text/html; charset=utf-8", index_html_start, index_html_end);
}

static esp_err_t css_handler(httpd_req_t *request)
{
    return send_asset(request, "text/css; charset=utf-8", app_css_start, app_css_end);
}

static esp_err_t javascript_handler(httpd_req_t *request)
{
    return send_asset(request, "application/javascript; charset=utf-8", app_js_start, app_js_end);
}

static bool get_header(httpd_req_t *request, const char *name, char *value, size_t value_capacity)
{
    size_t length = httpd_req_get_hdr_value_len(request, name);
    return length > 0U && length < value_capacity &&
           httpd_req_get_hdr_value_str(request, name, value, value_capacity) == ESP_OK;
}

static bool same_origin(httpd_req_t *request)
{
    char origin[DESK_HTTP_HEADER_MAX_SIZE];
    char host[DESK_HTTP_HEADER_MAX_SIZE];
    return get_header(request, "Origin", origin, sizeof(origin)) &&
           get_header(request, "Host", host, sizeof(host)) &&
           desk_http_origin_matches_host(origin, host);
}

static bool read_body(httpd_req_t *request, char body[DESK_HTTP_BODY_MAX_SIZE + 1U],
                      size_t *body_length)
{
    if (request->content_len == 0U || request->content_len > DESK_HTTP_BODY_MAX_SIZE) {
        return false;
    }

    size_t received = 0U;
    unsigned timeouts = 0U;
    while (received < request->content_len) {
        int result = httpd_req_recv(request, &body[received], request->content_len - received);
        if (result == HTTPD_SOCK_ERR_TIMEOUT && timeouts < DESK_HTTP_RECEIVE_TIMEOUT_LIMIT) {
            ++timeouts;
            continue;
        }
        if (result <= 0) {
            return false;
        }
        received += (size_t)result;
    }
    body[received] = '\0';
    *body_length = received;
    return true;
}

static bool form_content_type(httpd_req_t *request)
{
    char content_type[64];
    return get_header(request, "Content-Type", content_type, sizeof(content_type)) &&
           strcmp(content_type, "application/x-www-form-urlencoded") == 0;
}

static bool binary_content_type(httpd_req_t *request)
{
    char content_type[64];
    return get_header(request, "Content-Type", content_type, sizeof(content_type)) &&
           strcmp(content_type, "application/octet-stream") == 0;
}

static bool session_from_request(httpd_req_t *request, char session[DESK_AUTH_SECRET_TEXT_SIZE])
{
    char cookie_header[DESK_HTTP_HEADER_MAX_SIZE];
    if (!get_header(request, "Cookie", cookie_header, sizeof(cookie_header))) {
        return false;
    }
    return desk_http_extract_cookie(cookie_header, strlen(cookie_header), SESSION_COOKIE, session,
                                    DESK_AUTH_SECRET_TEXT_SIZE) == DESK_HTTP_COOKIE_OK;
}

static bool request_is_authenticated(esp_http_api_t *api, httpd_req_t *request)
{
    char session[DESK_AUTH_SECRET_TEXT_SIZE];
    return session_from_request(request, session) &&
           desk_authenticator_validate_session(&api->authenticator, session, now_ms());
}

static bool desk_is_stopped(const desk_app_status_t *status)
{
    for (size_t axis = 0U; axis < DESK_AXIS_COUNT; ++axis) {
        if (status->outputs.motor[axis].enable ||
            status->outputs.motor[axis].direction != MOTOR_COMMAND_STOP) {
            return false;
        }
    }
    return true;
}

static bool wait_for_motion_inhibit(void)
{
    for (uint32_t elapsed = 0U; elapsed <= DESK_HTTP_INHIBIT_TIMEOUT_MS;
         elapsed += DESK_HTTP_INHIBIT_POLL_MS) {
        desk_app_status_t status;
        if (desk_app_get_runtime_status(&status) && status.motion_inhibited &&
            desk_is_stopped(&status)) {
            return true;
        }
        if (elapsed < DESK_HTTP_INHIBIT_TIMEOUT_MS) {
            vTaskDelay(pdMS_TO_TICKS(DESK_HTTP_INHIBIT_POLL_MS));
        }
    }
    return false;
}

static const char *state_name(desk_state_t state)
{
    switch (state) {
    case DESK_STATE_SAFE_START:
        return "safe_start";
    case DESK_STATE_UNREFERENCED:
        return "unreferenced";
    case DESK_STATE_IDLE:
        return "idle";
    case DESK_STATE_MOVING_UP:
        return "moving_up";
    case DESK_STATE_MOVING_DOWN:
        return "moving_down";
    case DESK_STATE_CALIBRATING_DOWN:
        return "calibrating_down";
    case DESK_STATE_ERROR:
        return "error";
    }
    return "unknown";
}

static const char *error_name(desk_error_t error)
{
    switch (error) {
    case DESK_ERROR_NONE:
        return "none";
    case DESK_ERROR_EMERGENCY_STOP:
        return "emergency_stop";
    case DESK_ERROR_BOTH_BUTTONS_PRESSED:
        return "both_buttons";
    case DESK_ERROR_ALIGNMENT_TIMEOUT:
        return "alignment_timeout";
    case DESK_ERROR_CALIBRATION_TIMEOUT:
        return "calibration_timeout";
    case DESK_ERROR_LIMIT_CONFLICT:
        return "limit_conflict";
    case DESK_ERROR_INVALID_TRANSITION:
        return "invalid_transition";
    case DESK_ERROR_HARDWARE_FAULT:
        return "hardware_fault";
    case DESK_ERROR_CONTROL_LOOP_OVERRUN:
        return "control_loop_overrun";
    case DESK_ERROR_FEEDBACK_MISSING:
        return "feedback_missing";
    case DESK_ERROR_FEEDBACK_DIRECTION:
        return "feedback_direction";
    case DESK_ERROR_FEEDBACK_STALL:
        return "feedback_stall";
    case DESK_ERROR_FEEDBACK_SKEW:
        return "feedback_skew";
    }
    return "unknown";
}

static const char *mode_name(desk_control_mode_t mode)
{
    switch (mode) {
    case DESK_CONTROL_MODE_LOCAL_ONLY:
        return "local";
    case DESK_CONTROL_MODE_HYBRID:
        return "hybrid";
    case DESK_CONTROL_MODE_MOBILE_ONLY:
        return "mobile";
    }
    return "unknown";
}

static const char *source_name(desk_control_source_t source)
{
    switch (source) {
    case DESK_CONTROL_SOURCE_NONE:
        return "none";
    case DESK_CONTROL_SOURCE_LOCAL:
        return "local";
    case DESK_CONTROL_SOURCE_REMOTE:
        return "remote";
    }
    return "unknown";
}

static esp_err_t status_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!request_is_authenticated(api, request)) {
        return send_error(request, "401 Unauthorized", "authentication_required");
    }

    desk_app_status_t status;
    desk_runtime_diagnostics_t diagnostics;
    if (!desk_app_get_runtime_status(&status) || !desk_app_get_runtime_diagnostics(&diagnostics)) {
        return send_error(request, "503 Service Unavailable", "control_unavailable");
    }

    char response[2048];
    const esp_app_desc_t *app_description = esp_app_get_description();
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    int written = snprintf(
        response, sizeof(response),
        "{\"firmware\":\"%s\",\"partition\":\"%s\",\"reset_reason\":%u,"
        "\"state\":\"%s\",\"error\":\"%s\",\"mode\":\"%s\","
        "\"source\":\"%s\",\"remote_active\":%s,\"referenced\":%s,"
        "\"feedback_warning\":%s,\"emergency\":%s,\"provisioning\":%s,"
        "\"wifi_connected\":%s,\"preset_target_reached\":%s,"
        "\"preset_storage_fault\":%s,\"motion_inhibited\":%s,\"presets\":["
        "{\"valid\":%s,\"target_um\":%ld},"
        "{\"valid\":%s,\"target_um\":%ld},"
        "{\"valid\":%s,\"target_um\":%ld}],\"axes\":["
        "{\"position_um\":%ld,\"hall\":%ld,\"hall_valid\":%s,"
        "\"upper\":%s,\"lower\":%s,\"direction\":%u,\"duty\":%u,\"enabled\":%s},"
        "{\"position_um\":%ld,\"hall\":%ld,\"hall_valid\":%s,"
        "\"upper\":%s,\"lower\":%s,\"direction\":%u,\"duty\":%u,\"enabled\":%s},"
        "{\"position_um\":%ld,\"hall\":%ld,\"hall_valid\":%s,"
        "\"upper\":%s,\"lower\":%s,\"direction\":%u,\"duty\":%u,\"enabled\":%s}],"
        "\"diagnostics\":{\"free_heap\":%u,\"minimum_free_heap\":%u,"
        "\"dropped_logs\":%lu,\"control_stack\":%lu,\"logger_stack\":%lu}}",
        app_description->version, running_partition->label, (unsigned)esp_reset_reason(),
        state_name(status.outputs.state), error_name(status.outputs.error),
        mode_name(status.control_mode), source_name(status.control_source),
        status.remote_control_active ? "true" : "false",
        status.outputs.position_reference_valid ? "true" : "false",
        status.outputs.feedback_warning ? "true" : "false",
        status.inputs.emergency_active ? "true" : "false",
        esp_wifi_service_provisioning_active(api->wifi) ? "true" : "false",
        esp_wifi_service_station_connected(api->wifi) ? "true" : "false",
        status.preset_target_reached ? "true" : "false",
        api->store->preset_storage_fault ? "true" : "false",
        status.motion_inhibited ? "true" : "false", status.presets.valid[0] ? "true" : "false",
        (long)status.presets.target_um[0], status.presets.valid[1] ? "true" : "false",
        (long)status.presets.target_um[1], status.presets.valid[2] ? "true" : "false",
        (long)status.presets.target_um[2], (long)status.inputs.axis[0].position_um,
        (long)status.inputs.axis[0].hall_count, status.inputs.axis[0].hall_valid ? "true" : "false",
        status.inputs.axis[0].upper_limit_active ? "true" : "false",
        status.inputs.axis[0].lower_limit_active ? "true" : "false",
        (unsigned)status.outputs.motor[0].direction,
        (unsigned)status.outputs.motor[0].duty_permille,
        status.outputs.motor[0].enable ? "true" : "false", (long)status.inputs.axis[1].position_um,
        (long)status.inputs.axis[1].hall_count, status.inputs.axis[1].hall_valid ? "true" : "false",
        status.inputs.axis[1].upper_limit_active ? "true" : "false",
        status.inputs.axis[1].lower_limit_active ? "true" : "false",
        (unsigned)status.outputs.motor[1].direction,
        (unsigned)status.outputs.motor[1].duty_permille,
        status.outputs.motor[1].enable ? "true" : "false", (long)status.inputs.axis[2].position_um,
        (long)status.inputs.axis[2].hall_count, status.inputs.axis[2].hall_valid ? "true" : "false",
        status.inputs.axis[2].upper_limit_active ? "true" : "false",
        status.inputs.axis[2].lower_limit_active ? "true" : "false",
        (unsigned)status.outputs.motor[2].direction,
        (unsigned)status.outputs.motor[2].duty_permille,
        status.outputs.motor[2].enable ? "true" : "false",
        (unsigned)diagnostics.current_free_heap_bytes,
        (unsigned)diagnostics.minimum_free_heap_bytes,
        (unsigned long)diagnostics.dropped_log_records,
        (unsigned long)diagnostics.control_stack_high_water_words,
        (unsigned long)diagnostics.logger_stack_high_water_words);
    if (written <= 0 || (size_t)written >= sizeof(response)) {
        return send_error(request, HTTPD_500, "status_overflow");
    }
    return send_json(request, HTTPD_200, response);
}

static esp_err_t ota_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !request_is_authenticated(api, request) ||
        !binary_content_type(request)) {
        return send_error(request, "403 Forbidden", "update_forbidden");
    }

    desk_app_status_t status;
    if (!desk_app_get_runtime_status(&status) || !desk_is_stopped(&status) ||
        status.remote_control_active || request->content_len == 0U) {
        return send_error(request, "409 Conflict", "desk_must_be_stopped");
    }
    if (!desk_app_queue_motion_inhibit(true) || !wait_for_motion_inhibit()) {
        return send_error(request, "503 Service Unavailable", "maintenance_inhibit_failed");
    }

    esp_ota_update_t update;
    esp_err_t result = esp_ota_update_begin(&update, request->content_len);
    uint8_t chunk[DESK_HTTP_OTA_CHUNK_SIZE];
    size_t received = 0U;
    unsigned timeouts = 0U;
    while (result == ESP_OK && received < request->content_len) {
        size_t remaining = request->content_len - received;
        size_t requested = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        int chunk_size = httpd_req_recv(request, (char *)chunk, requested);
        if (chunk_size == HTTPD_SOCK_ERR_TIMEOUT && timeouts < DESK_HTTP_RECEIVE_TIMEOUT_LIMIT) {
            ++timeouts;
            continue;
        }
        if (chunk_size <= 0) {
            result = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        timeouts = 0U;
        result = esp_ota_update_write(&update, chunk, (size_t)chunk_size);
        if (result == ESP_OK) {
            received += (size_t)chunk_size;
        }
    }
    if (result == ESP_OK) {
        result = esp_ota_update_finish(&update);
    } else {
        esp_ota_update_abort(&update);
    }
    if (result != ESP_OK) {
        esp_ota_update_abort(&update);
        (void)desk_app_queue_motion_inhibit(false);
        ESP_LOGE(TAG, "signed firmware update rejected: %s", esp_err_to_name(result));
        return send_error(request, HTTPD_400, "update_rejected");
    }

    ESP_LOGW(TAG, "signed firmware staged in %s; rebooting", update.partition->label);
    result = send_json(request, HTTPD_200, "{\"rebooting\":true}");
    if (result == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(200U));
        esp_restart();
    }
    return result;
}

static esp_err_t diagnostics_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!request_is_authenticated(api, request)) {
        return send_error(request, "401 Unauthorized", "authentication_required");
    }

    desk_diagnostic_log_t log;
    bool recovered_storage_fault;
    if (!desk_app_get_runtime_diagnostic_log(&log, &recovered_storage_fault) ||
        set_security_headers(request) != ESP_OK ||
        httpd_resp_set_type(request, HTTPD_TYPE_JSON) != ESP_OK) {
        return send_error(request, "503 Service Unavailable", "diagnostics_unavailable");
    }

    const char *prefix = recovered_storage_fault ? "{\"storage_fault\":true,\"events\":["
                                                 : "{\"storage_fault\":false,\"events\":[";
    esp_err_t result = httpd_resp_send_chunk(request, prefix, HTTPD_RESP_USE_STRLEN);
    for (size_t offset = 0U; result == ESP_OK && offset < log.count; ++offset) {
        desk_diagnostic_event_t event;
        char encoded[192];
        if (!desk_diagnostic_log_get_newest(&log, offset, &event)) {
            result = ESP_ERR_INVALID_STATE;
            break;
        }
        int written =
            snprintf(encoded, sizeof(encoded),
                     "%s{\"sequence\":%lu,\"reset_reason\":%lu,\"uptime_s\":%lu,"
                     "\"state\":\"%s\",\"error\":\"%s\"}",
                     offset == 0U ? "" : ",", (unsigned long)event.sequence,
                     (unsigned long)event.reset_reason, (unsigned long)event.uptime_seconds,
                     state_name(event.state), error_name(event.error));
        if (written <= 0 || (size_t)written >= sizeof(encoded)) {
            result = ESP_ERR_INVALID_SIZE;
            break;
        }
        result = httpd_resp_send_chunk(request, encoded, (ssize_t)written);
    }
    if (result == ESP_OK) {
        result = httpd_resp_send_chunk(request, "]}", HTTPD_RESP_USE_STRLEN);
    }
    if (result == ESP_OK) {
        result = httpd_resp_send_chunk(request, NULL, 0U);
    }
    return result;
}

static esp_err_t preset_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !request_is_authenticated(api, request) ||
        !form_content_type(request)) {
        return send_error(request, "403 Forbidden", "preset_forbidden");
    }

    desk_app_status_t status;
    if (!desk_app_get_runtime_status(&status) || !desk_is_stopped(&status) ||
        status.outputs.state != DESK_STATE_IDLE || status.outputs.error != DESK_ERROR_NONE) {
        return send_error(request, "409 Conflict", "desk_must_be_idle");
    }

    char body[DESK_HTTP_BODY_MAX_SIZE + 1U];
    size_t body_length;
    char encoded_slot[2];
    size_t slot_length;
    if (!read_body(request, body, &body_length) ||
        desk_form_get_value(body, body_length, "slot", encoded_slot, sizeof(encoded_slot),
                            &slot_length) != DESK_FORM_OK ||
        slot_length != 1U || encoded_slot[0] < '1' || encoded_slot[0] > '3') {
        return send_error(request, HTTPD_400, "invalid_preset_slot");
    }
    size_t slot = (size_t)(encoded_slot[0] - '1');
    desk_preset_bank_t updated = status.presets;
    if (!desk_preset_bank_capture(&updated, slot, &status.inputs,
                                  status.outputs.position_reference_valid,
                                  status.outputs.feedback_warning,
                                  (int32_t)BOARD_ACTUATOR_TRAVEL_UM, BOARD_HALL_STOP_UM)) {
        return send_error(request, "409 Conflict", "preset_capture_unsafe");
    }

    desk_preset_bank_t previous = api->store->presets;
    esp_err_t result = esp_network_store_save_presets(api->store, &updated);
    if (result != ESP_OK || !desk_app_queue_preset_target(slot, updated.target_um[slot])) {
        if (result == ESP_OK && esp_network_store_save_presets(api->store, &previous) != ESP_OK) {
            ESP_LOGE(TAG, "preset rollback failed");
        }
        return send_error(request, "503 Service Unavailable", "preset_apply_failed");
    }
    return send_json(request, HTTPD_200, "{\"updated\":true}");
}

static esp_err_t login_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !form_content_type(request)) {
        return send_error(request, HTTPD_400, "invalid_request");
    }

    char body[DESK_HTTP_BODY_MAX_SIZE + 1U];
    size_t body_length;
    char token[DESK_AUTH_SECRET_TEXT_SIZE];
    size_t token_length;
    if (!read_body(request, body, &body_length) ||
        desk_form_get_value(body, body_length, "token", token, sizeof(token), &token_length) !=
            DESK_FORM_OK ||
        token_length != DESK_AUTH_SECRET_HEX_LENGTH) {
        return send_error(request, HTTPD_400, "invalid_credentials");
    }

    uint8_t random[DESK_AUTH_SECRET_BYTES];
    char session[DESK_AUTH_SECRET_TEXT_SIZE];
    desk_auth_login_result_t login_result = DESK_AUTH_LOGIN_INVALID_RANDOM;
    for (unsigned attempt = 0U; attempt < 4U && login_result == DESK_AUTH_LOGIN_INVALID_RANDOM;
         ++attempt) {
        esp_fill_random(random, sizeof(random));
        login_result =
            desk_authenticator_login(&api->authenticator, token, random, now_ms(), session);
    }
    if (login_result == DESK_AUTH_LOGIN_RATE_LIMITED) {
        return send_error(request, "429 Too Many Requests", "rate_limited");
    }
    if (login_result == DESK_AUTH_LOGIN_DENIED) {
        return send_error(request, "401 Unauthorized", "invalid_credentials");
    }
    if (login_result != DESK_AUTH_LOGIN_OK) {
        return send_error(request, "503 Service Unavailable", "session_unavailable");
    }

    char cookie[128];
    int written = snprintf(cookie, sizeof(cookie),
                           "%s=%s; Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=28800",
                           SESSION_COOKIE, session);
    if (written <= 0 || (size_t)written >= sizeof(cookie) ||
        set_security_headers(request) != ESP_OK ||
        httpd_resp_set_hdr(request, "Set-Cookie", cookie) != ESP_OK) {
        (void)desk_authenticator_logout(&api->authenticator, session);
        return ESP_FAIL;
    }
    return send_json(request, HTTPD_200, "{\"authenticated\":true}");
}

static esp_err_t logout_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    char session[DESK_AUTH_SECRET_TEXT_SIZE];
    if (!same_origin(request) || !session_from_request(request, session) ||
        !desk_authenticator_logout(&api->authenticator, session)) {
        return send_error(request, "401 Unauthorized", "authentication_required");
    }
    if (httpd_resp_set_hdr(request, "Set-Cookie",
                           "desk_session=expired; Path=/; Secure; HttpOnly; SameSite=Strict; "
                           "Max-Age=0") != ESP_OK) {
        return ESP_FAIL;
    }
    return send_json(request, HTTPD_200, "{\"authenticated\":false}");
}

static esp_err_t provisioning_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !request_is_authenticated(api, request) ||
        !esp_wifi_service_provisioning_active(api->wifi) || !form_content_type(request)) {
        return send_error(request, "403 Forbidden", "provisioning_forbidden");
    }

    desk_app_status_t status;
    if (!desk_app_get_runtime_status(&status) || !desk_is_stopped(&status)) {
        return send_error(request, "409 Conflict", "desk_must_be_stopped");
    }

    char body[DESK_HTTP_BODY_MAX_SIZE + 1U];
    size_t body_length;
    char ssid[DESK_WIFI_SSID_MAX_LENGTH + 1U];
    char password[DESK_WIFI_PASSWORD_MAX_LENGTH + 1U];
    size_t ssid_length;
    size_t password_length;
    if (!read_body(request, body, &body_length) ||
        desk_form_get_value(body, body_length, "ssid", ssid, sizeof(ssid), &ssid_length) !=
            DESK_FORM_OK ||
        desk_form_get_value(body, body_length, "password", password, sizeof(password),
                            &password_length) != DESK_FORM_OK ||
        !desk_network_credentials_are_valid(ssid, ssid_length, password, password_length)) {
        return send_error(request, HTTPD_400, "invalid_network");
    }

    esp_err_t result =
        esp_network_store_save_wifi(api->store, ssid, ssid_length, password, password_length);
    if (result == ESP_OK) {
        result = esp_wifi_service_apply_credentials(api->wifi);
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "network apply failed: %s", esp_err_to_name(result));
        return send_error(request, "503 Service Unavailable", "network_apply_failed");
    }
    return send_json(request, HTTPD_200, "{\"connecting\":true}");
}

static bool parse_control_mode(const char *text, desk_control_mode_t *mode)
{
    if (strcmp(text, "local") == 0) {
        *mode = DESK_CONTROL_MODE_LOCAL_ONLY;
        return true;
    }
    if (strcmp(text, "hybrid") == 0) {
        *mode = DESK_CONTROL_MODE_HYBRID;
        return true;
    }
    if (strcmp(text, "mobile") == 0) {
        *mode = DESK_CONTROL_MODE_MOBILE_ONLY;
        return true;
    }
    return false;
}

static esp_err_t mode_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !request_is_authenticated(api, request) ||
        !form_content_type(request)) {
        return send_error(request, "403 Forbidden", "mode_forbidden");
    }

    desk_app_status_t status;
    if (!desk_app_get_runtime_status(&status) || !desk_is_stopped(&status)) {
        return send_error(request, "409 Conflict", "desk_must_be_stopped");
    }
    char body[DESK_HTTP_BODY_MAX_SIZE + 1U];
    size_t body_length;
    char encoded_mode[16];
    size_t mode_length;
    desk_control_mode_t mode;
    if (!read_body(request, body, &body_length) ||
        desk_form_get_value(body, body_length, "mode", encoded_mode, sizeof(encoded_mode),
                            &mode_length) != DESK_FORM_OK ||
        mode_length == 0U || !parse_control_mode(encoded_mode, &mode)) {
        return send_error(request, HTTPD_400, "invalid_mode");
    }

    desk_control_mode_t previous = api->store->control_mode;
    esp_err_t result = esp_network_store_save_control_mode(api->store, mode);
    if (result != ESP_OK || !desk_app_queue_control_mode(mode)) {
        if (result == ESP_OK) {
            (void)esp_network_store_save_control_mode(api->store, previous);
        }
        return send_error(request, "503 Service Unavailable", "mode_apply_failed");
    }
    return send_json(request, HTTPD_200, "{\"updated\":true}");
}

static esp_err_t clear_error_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (!same_origin(request) || !request_is_authenticated(api, request)) {
        return send_error(request, "403 Forbidden", "clear_error_forbidden");
    }

    desk_app_status_t status;
    if (!desk_app_get_runtime_status(&status)) {
        return send_error(request, "503 Service Unavailable", "control_unavailable");
    }
    if (status.outputs.state != DESK_STATE_ERROR || status.inputs.emergency_active ||
        !desk_is_stopped(&status)) {
        return send_error(request, "409 Conflict", "error_not_clearable");
    }
    if (!desk_app_queue_clear_error()) {
        return send_error(request, "503 Service Unavailable", "clear_error_queue_full");
    }
    return send_json(request, HTTPD_200, "{\"accepted\":true}");
}

static bool generate_lease_id(uint64_t *lease_id)
{
    for (unsigned attempt = 0U; attempt < 4U; ++attempt) {
        esp_fill_random(lease_id, sizeof(*lease_id));
        if (*lease_id != 0U) {
            return true;
        }
    }
    return false;
}

static esp_err_t websocket_pre_handshake(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    int socket_fd = httpd_req_to_sockfd(request);
    desk_app_status_t status;
    if (!same_origin(request) || !request_is_authenticated(api, request) ||
        !desk_app_get_runtime_status(&status) ||
        status.control_mode == DESK_CONTROL_MODE_LOCAL_ONLY || socket_fd < 0 ||
        api->websocket_fd >= 0) {
        return ESP_FAIL;
    }

    uint64_t lease_id;
    if (!generate_lease_id(&lease_id) || !desk_app_queue_remote_lease(lease_id)) {
        return ESP_FAIL;
    }
    api->websocket_fd = socket_fd;
    api->websocket_lease_id = lease_id;
    return ESP_OK;
}

static esp_err_t websocket_handler(httpd_req_t *request)
{
    esp_http_api_t *api = request->user_ctx;
    if (request->method == HTTP_GET) {
        return ESP_OK;
    }
    if (httpd_req_to_sockfd(request) != api->websocket_fd || api->websocket_lease_id == 0U) {
        return ESP_FAIL;
    }

    httpd_ws_frame_t frame = {0};
    esp_err_t result = httpd_ws_recv_frame(request, &frame, 0U);
    if (result != ESP_OK || frame.len != DESK_CONTROL_COMMAND_FRAME_SIZE) {
        return ESP_FAIL;
    }
    uint8_t payload[DESK_CONTROL_COMMAND_FRAME_SIZE];
    frame.payload = payload;
    result = httpd_ws_recv_frame(request, &frame, sizeof(payload));
    if (result != ESP_OK || frame.type != HTTPD_WS_TYPE_BINARY || !frame.final ||
        frame.fragmented) {
        return ESP_FAIL;
    }

    desk_remote_command_t command;
    if (desk_control_protocol_parse_command(payload, frame.len, api->websocket_lease_id,
                                            &command) != DESK_PROTOCOL_PARSE_OK ||
        !desk_app_queue_remote_command(&command)) {
        return ESP_FAIL;
    }
    return command.action == DESK_CONTROL_ACTION_STOP ? ESP_FAIL : ESP_OK;
}

static void http_session_close(httpd_handle_t server, int socket_fd)
{
    esp_http_api_t *api = httpd_get_global_user_ctx(server);
    if (api != NULL && socket_fd == api->websocket_fd) {
        (void)desk_app_queue_remote_release(api->websocket_lease_id);
        api->websocket_fd = -1;
        api->websocket_lease_id = 0U;
    }
}

static void keep_static_context(void *context)
{
    (void)context;
}

static esp_err_t register_handler(httpd_handle_t server, const httpd_uri_t *handler)
{
    esp_err_t result = httpd_register_uri_handler(server, handler);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "failed to register %s: %s", handler->uri, esp_err_to_name(result));
    }
    return result;
}

esp_err_t esp_http_api_start(esp_http_api_t *api, esp_network_store_t *store,
                             esp_wifi_service_t *wifi)
{
    if (api == NULL || store == NULL || !store->initialized || wifi == NULL || !wifi->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    *api = (esp_http_api_t){
        .store = store,
        .wifi = wifi,
        .websocket_fd = -1,
    };
    desk_authenticator_init(&api->authenticator, store->admin_secret);
    if (!api->authenticator.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    desk_build_secrets_t build_secrets;
    if (!desk_build_secrets_get(&build_secrets)) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.stack_size = 12288U;
    config.httpd.max_uri_handlers = 14U;
    config.httpd.max_open_sockets = 4U;
    config.httpd.max_resp_headers = 12U;
    config.httpd.lru_purge_enable = true;
    config.httpd.global_user_ctx = api;
    config.httpd.global_user_ctx_free_fn = keep_static_context;
    config.httpd.close_fn = http_session_close;
    config.servercert = build_secrets.tls_certificate;
    config.servercert_len = build_secrets.tls_certificate_size;
    config.prvtkey_pem = build_secrets.tls_private_key;
    config.prvtkey_len = build_secrets.tls_private_key_size;
    config.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
    config.port_secure = 443U;
    config.tls_version = ESP_TLS_VER_TLS_1_2;

    esp_err_t result = httpd_ssl_start(&api->server, &config);
    if (result != ESP_OK) {
        return result;
    }

    const httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = api},
        {.uri = "/app.css", .method = HTTP_GET, .handler = css_handler, .user_ctx = api},
        {.uri = "/app.js", .method = HTTP_GET, .handler = javascript_handler, .user_ctx = api},
        {.uri = "/api/v1/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = api},
        {.uri = "/api/v1/diagnostics",
         .method = HTTP_GET,
         .handler = diagnostics_handler,
         .user_ctx = api},
        {.uri = "/api/v1/session", .method = HTTP_POST, .handler = login_handler, .user_ctx = api},
        {.uri = "/api/v1/session",
         .method = HTTP_DELETE,
         .handler = logout_handler,
         .user_ctx = api},
        {.uri = "/api/v1/provision",
         .method = HTTP_POST,
         .handler = provisioning_handler,
         .user_ctx = api},
        {.uri = "/api/v1/mode", .method = HTTP_POST, .handler = mode_handler, .user_ctx = api},
        {.uri = "/api/v1/error/clear",
         .method = HTTP_POST,
         .handler = clear_error_handler,
         .user_ctx = api},
        {.uri = "/api/v1/preset", .method = HTTP_POST, .handler = preset_handler, .user_ctx = api},
        {.uri = "/api/v1/update", .method = HTTP_POST, .handler = ota_handler, .user_ctx = api},
        {.uri = "/api/v1/control",
         .method = HTTP_GET,
         .handler = websocket_handler,
         .user_ctx = api,
         .is_websocket = true,
         .handle_ws_control_frames = false,
         .supported_subprotocol = DESK_HTTP_WEBSOCKET_PROTOCOL,
         .ws_pre_handshake_cb = websocket_pre_handshake},
    };
    for (size_t index = 0U; index < sizeof(handlers) / sizeof(handlers[0]); ++index) {
        result = register_handler(api->server, &handlers[index]);
        if (result != ESP_OK) {
            (void)httpd_ssl_stop(api->server);
            api->server = NULL;
            return result;
        }
    }
    api->initialized = true;
    ESP_LOGI(TAG, "authenticated HTTPS/WSS API started");
    return ESP_OK;
}
