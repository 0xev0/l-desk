#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DESK_REMOTE_COMMAND_MAX_LEASE_MS 250U

typedef enum {
    DESK_CONTROL_MODE_LOCAL_ONLY = 0,
    DESK_CONTROL_MODE_HYBRID,
    DESK_CONTROL_MODE_MOBILE_ONLY,
} desk_control_mode_t;

typedef enum {
    DESK_CONTROL_SOURCE_NONE = 0,
    DESK_CONTROL_SOURCE_LOCAL,
    DESK_CONTROL_SOURCE_REMOTE,
} desk_control_source_t;

typedef enum {
    DESK_CONTROL_ACTION_STOP = 0,
    DESK_CONTROL_ACTION_UP,
    DESK_CONTROL_ACTION_DOWN,
    DESK_CONTROL_ACTION_CALIBRATE,
    DESK_CONTROL_ACTION_PRESET_1,
    DESK_CONTROL_ACTION_PRESET_2,
    DESK_CONTROL_ACTION_PRESET_3,
} desk_control_action_t;

typedef struct {
    bool up_pressed;
    bool down_pressed;
    bool calibrate_pressed;
} desk_local_controls_t;

typedef struct {
    uint64_t lease_id;
    uint32_t sequence;
    uint32_t valid_for_ms;
    desk_control_action_t action;
} desk_remote_command_t;

typedef struct {
    desk_control_mode_t mode;
    desk_control_action_t remote_action;
    uint64_t remote_lease_id;
    uint64_t remote_expires_at_ms;
    uint32_t last_remote_sequence;
    bool remote_lease_authorized;
    bool remote_active;
} desk_command_arbiter_t;

typedef struct {
    bool up_pressed;
    bool down_pressed;
    bool calibrate_pressed;
    bool remote_active;
    bool remote_expired;
    bool remote_preempted;
    desk_control_action_t action;
    desk_control_source_t source;
} desk_resolved_controls_t;

void desk_command_arbiter_init(desk_command_arbiter_t *arbiter, desk_control_mode_t mode);
bool desk_command_arbiter_set_mode(desk_command_arbiter_t *arbiter, desk_control_mode_t mode);
bool desk_command_arbiter_acquire_remote_lease(desk_command_arbiter_t *arbiter, uint64_t lease_id);
bool desk_command_arbiter_submit_remote(desk_command_arbiter_t *arbiter,
                                        const desk_remote_command_t *command, uint64_t now_ms);
bool desk_command_arbiter_release_remote_lease(desk_command_arbiter_t *arbiter, uint64_t lease_id);
void desk_command_arbiter_cancel_remote(desk_command_arbiter_t *arbiter);
desk_resolved_controls_t desk_command_arbiter_resolve(desk_command_arbiter_t *arbiter,
                                                      const desk_local_controls_t *local,
                                                      uint64_t now_ms);
