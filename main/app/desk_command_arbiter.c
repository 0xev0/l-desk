#include "desk_command_arbiter.h"

#include <limits.h>
#include <stddef.h>

static bool mode_is_valid(desk_control_mode_t mode)
{
    return mode == DESK_CONTROL_MODE_LOCAL_ONLY || mode == DESK_CONTROL_MODE_HYBRID ||
           mode == DESK_CONTROL_MODE_MOBILE_ONLY;
}

static bool action_is_valid(desk_control_action_t action)
{
    return action == DESK_CONTROL_ACTION_STOP || action == DESK_CONTROL_ACTION_UP ||
           action == DESK_CONTROL_ACTION_DOWN || action == DESK_CONTROL_ACTION_CALIBRATE;
}

static uint64_t saturating_deadline(uint64_t now_ms, uint32_t duration_ms)
{
    return UINT64_MAX - now_ms < duration_ms ? UINT64_MAX : now_ms + duration_ms;
}

static bool local_is_active(const desk_local_controls_t *local)
{
    return local->up_pressed || local->down_pressed || local->calibrate_pressed;
}

static void apply_action(desk_resolved_controls_t *resolved, desk_control_action_t action)
{
    switch (action) {
    case DESK_CONTROL_ACTION_STOP:
        break;
    case DESK_CONTROL_ACTION_UP:
        resolved->up_pressed = true;
        break;
    case DESK_CONTROL_ACTION_DOWN:
        resolved->down_pressed = true;
        break;
    case DESK_CONTROL_ACTION_CALIBRATE:
        resolved->calibrate_pressed = true;
        break;
    }
}

void desk_command_arbiter_cancel_remote(desk_command_arbiter_t *arbiter)
{
    if (arbiter == NULL) {
        return;
    }
    arbiter->remote_action = DESK_CONTROL_ACTION_STOP;
    arbiter->remote_lease_id = 0U;
    arbiter->remote_expires_at_ms = 0U;
    arbiter->last_remote_sequence = 0U;
    arbiter->remote_lease_authorized = false;
    arbiter->remote_active = false;
}

void desk_command_arbiter_init(desk_command_arbiter_t *arbiter, desk_control_mode_t mode)
{
    if (arbiter == NULL) {
        return;
    }
    *arbiter = (desk_command_arbiter_t){
        .mode = mode_is_valid(mode) ? mode : DESK_CONTROL_MODE_LOCAL_ONLY,
    };
}

bool desk_command_arbiter_set_mode(desk_command_arbiter_t *arbiter, desk_control_mode_t mode)
{
    if (arbiter == NULL || !mode_is_valid(mode)) {
        return false;
    }
    desk_command_arbiter_cancel_remote(arbiter);
    arbiter->mode = mode;
    return true;
}

bool desk_command_arbiter_acquire_remote_lease(desk_command_arbiter_t *arbiter,
                                               uint64_t lease_id)
{
    if (arbiter == NULL || arbiter->mode == DESK_CONTROL_MODE_LOCAL_ONLY || lease_id == 0U ||
        arbiter->remote_lease_authorized) {
        return false;
    }
    arbiter->remote_lease_id = lease_id;
    arbiter->last_remote_sequence = 0U;
    arbiter->remote_lease_authorized = true;
    return true;
}

static bool expire_remote_if_needed(desk_command_arbiter_t *arbiter, uint64_t now_ms)
{
    if (!arbiter->remote_active || now_ms < arbiter->remote_expires_at_ms) {
        return false;
    }
    desk_command_arbiter_cancel_remote(arbiter);
    return true;
}

bool desk_command_arbiter_submit_remote(desk_command_arbiter_t *arbiter,
                                        const desk_remote_command_t *command,
                                        uint64_t now_ms)
{
    if (arbiter == NULL || command == NULL ||
        arbiter->mode == DESK_CONTROL_MODE_LOCAL_ONLY || command->lease_id == 0U ||
        command->sequence == 0U || !action_is_valid(command->action) ||
        command->valid_for_ms == 0U ||
        command->valid_for_ms > DESK_REMOTE_COMMAND_MAX_LEASE_MS ||
        !arbiter->remote_lease_authorized ||
        command->lease_id != arbiter->remote_lease_id) {
        return false;
    }

    (void)expire_remote_if_needed(arbiter, now_ms);
    if (!arbiter->remote_lease_authorized) {
        return false;
    }
    if (command->sequence <= arbiter->last_remote_sequence) {
        return false;
    }

    arbiter->last_remote_sequence = command->sequence;
    if (command->action == DESK_CONTROL_ACTION_STOP) {
        desk_command_arbiter_cancel_remote(arbiter);
        return true;
    }

    arbiter->remote_action = command->action;
    arbiter->remote_expires_at_ms = saturating_deadline(now_ms, command->valid_for_ms);
    arbiter->remote_active = true;
    return true;
}

desk_resolved_controls_t desk_command_arbiter_resolve(
    desk_command_arbiter_t *arbiter, const desk_local_controls_t *local, uint64_t now_ms)
{
    desk_resolved_controls_t resolved = {0};
    if (arbiter == NULL || local == NULL || !mode_is_valid(arbiter->mode)) {
        return resolved;
    }

    resolved.remote_expired = expire_remote_if_needed(arbiter, now_ms);
    bool use_local = arbiter->mode != DESK_CONTROL_MODE_MOBILE_ONLY && local_is_active(local);
    if (use_local) {
        resolved.up_pressed = local->up_pressed;
        resolved.down_pressed = local->down_pressed;
        resolved.calibrate_pressed = local->calibrate_pressed;
        resolved.source = DESK_CONTROL_SOURCE_LOCAL;
        if (arbiter->remote_active) {
            desk_command_arbiter_cancel_remote(arbiter);
            resolved.remote_preempted = true;
        }
        return resolved;
    }

    if (arbiter->mode != DESK_CONTROL_MODE_LOCAL_ONLY && arbiter->remote_active) {
        apply_action(&resolved, arbiter->remote_action);
        resolved.remote_active = true;
        resolved.source = DESK_CONTROL_SOURCE_REMOTE;
    }
    return resolved;
}
