#include "desk_app.h"

#include <limits.h>
#include <stddef.h>

#define DESK_SAFETY_INPUT_RELEASE_DEBOUNCE_MS 50U

static desk_outputs_t stopped_fault_outputs(void)
{
    desk_outputs_t outputs = {
        .state = DESK_STATE_ERROR,
        .error = DESK_ERROR_HARDWARE_FAULT,
    };
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis].direction = MOTOR_COMMAND_STOP;
    }
    return outputs;
}

static bool platform_is_complete(const desk_platform_t *platform)
{
    return platform != NULL && platform->initialize != NULL && platform->read_inputs != NULL &&
           platform->apply_outputs != NULL && platform->zero_hall_counts != NULL &&
           platform->service_watchdog != NULL && platform->now_ms != NULL;
}

static bool preset_slot_from_action(desk_control_action_t action, size_t *slot)
{
    switch (action) {
    case DESK_CONTROL_ACTION_PRESET_1:
        *slot = 0U;
        return true;
    case DESK_CONTROL_ACTION_PRESET_2:
        *slot = 1U;
        return true;
    case DESK_CONTROL_ACTION_PRESET_3:
        *slot = 2U;
        return true;
    case DESK_CONTROL_ACTION_STOP:
    case DESK_CONTROL_ACTION_UP:
    case DESK_CONTROL_ACTION_DOWN:
    case DESK_CONTROL_ACTION_CALIBRATE:
        return false;
    }
    return false;
}

static void apply_preset_action(desk_app_t *app, const desk_resolved_controls_t *controls,
                                desk_inputs_t *inputs)
{
    size_t slot;
    if (!preset_slot_from_action(controls->action, &slot)) {
        desk_preset_controller_reset(&app->preset_controller);
        return;
    }

    motor_command_t direction = MOTOR_COMMAND_STOP;
    bool valid =
        desk_preset_controller_update(&app->preset_controller, &app->presets, slot, inputs,
                                      app->last_outputs.position_reference_valid, &direction);
    inputs->up_pressed = valid && direction == MOTOR_COMMAND_UP;
    inputs->down_pressed = valid && direction == MOTOR_COMMAND_DOWN;
    inputs->calibrate_pressed = false;
}

static void update_clear_error_request(desk_app_t *app, const desk_inputs_t *inputs,
                                       uint64_t now_ms)
{
    if (app->controller.state != DESK_STATE_ERROR) {
        app->clear_error_timing = false;
        app->clear_error_armed = false;
        return;
    }

    bool unsafe_control = inputs->up_pressed || inputs->down_pressed || inputs->emergency_active ||
                          inputs->hardware_fault;
    if (unsafe_control) {
        app->clear_error_timing = false;
        app->clear_error_armed = false;
        return;
    }

    if (inputs->calibrate_pressed) {
        if (!app->clear_error_timing) {
            app->clear_error_timing = true;
            app->clear_error_pressed_since_ms = now_ms;
        } else if (now_ms >= app->clear_error_pressed_since_ms &&
                   now_ms - app->clear_error_pressed_since_ms >=
                       app->controller.config.calibrate_hold_ms) {
            app->clear_error_armed = true;
        }
        return;
    }

    if (app->clear_error_armed) {
        (void)desk_controller_request_clear_error(&app->controller, inputs, now_ms);
    }
    app->clear_error_timing = false;
    app->clear_error_armed = false;
}

bool desk_app_init(desk_app_t *app, const desk_platform_t *platform, const desk_config_t *config)
{
    if (app == NULL || !platform_is_complete(platform)) {
        return false;
    }

    *app = (desk_app_t){
        .platform = *platform,
        .last_outputs = stopped_fault_outputs(),
    };
    if (!app->platform.initialize(app->platform.context)) {
        return false;
    }

    desk_controller_init(&app->controller, config);
    desk_command_arbiter_init(&app->command_arbiter, DESK_CONTROL_MODE_HYBRID);
    desk_input_filter_init(&app->input_filter, DESK_SAFETY_INPUT_RELEASE_DEBOUNCE_MS);
    app->last_step_ms = app->platform.now_ms(app->platform.context);
    app->last_outputs.state = DESK_STATE_SAFE_START;
    app->last_outputs.error = DESK_ERROR_NONE;
    app->initialized = true;
    return true;
}

desk_outputs_t desk_app_step(desk_app_t *app)
{
    if (app == NULL || !app->initialized) {
        return stopped_fault_outputs();
    }

    uint64_t now_ms = app->platform.now_ms(app->platform.context);
    uint64_t elapsed = now_ms >= app->last_step_ms ? now_ms - app->last_step_ms : UINT64_MAX;
    uint32_t filter_elapsed = elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
    app->last_step_ms = now_ms;

    desk_inputs_t raw_inputs = {0};
    if (!app->platform.read_inputs(app->platform.context, &raw_inputs)) {
        raw_inputs.hardware_fault = true;
    }
    desk_inputs_t inputs =
        desk_input_filter_update(&app->input_filter, &raw_inputs, filter_elapsed);
    if (app->motion_release_pending && !inputs.up_pressed && !inputs.down_pressed &&
        !inputs.calibrate_pressed) {
        app->motion_inhibited = false;
        app->motion_release_pending = false;
    }
    desk_resolved_controls_t controls =
        desk_command_arbiter_resolve(&app->command_arbiter,
                                     &(desk_local_controls_t){
                                         .up_pressed = inputs.up_pressed,
                                         .down_pressed = inputs.down_pressed,
                                         .calibrate_pressed = inputs.calibrate_pressed,
                                     },
                                     now_ms);
    if (app->motion_inhibited) {
        desk_command_arbiter_cancel_remote(&app->command_arbiter);
        controls = (desk_resolved_controls_t){
            .action = DESK_CONTROL_ACTION_STOP,
        };
        desk_preset_controller_reset(&app->preset_controller);
    }
    inputs.up_pressed = controls.up_pressed;
    inputs.down_pressed = controls.down_pressed;
    inputs.calibrate_pressed = controls.calibrate_pressed;
    apply_preset_action(app, &controls, &inputs);
    app->last_control_source = controls.source;
    app->remote_control_active = controls.remote_active;
    app->last_inputs = inputs;
    update_clear_error_request(app, &inputs, now_ms);
    desk_outputs_t outputs = desk_controller_tick(&app->controller, &inputs, now_ms);

    if (outputs.calibration_completed && !app->platform.zero_hall_counts(app->platform.context)) {
        desk_controller_report_hardware_fault(&app->controller, now_ms);
        outputs = desk_controller_tick(&app->controller, &inputs, now_ms);
    }

    bool platform_fault = !app->platform.apply_outputs(app->platform.context, &outputs);
    if (!platform_fault) {
        platform_fault = !app->platform.service_watchdog(app->platform.context);
    }
    if (platform_fault) {
        desk_controller_report_hardware_fault(&app->controller, now_ms);
        outputs = desk_controller_tick(&app->controller, &inputs, now_ms);
        (void)app->platform.apply_outputs(app->platform.context, &outputs);
    }

    if (app->platform.log_transition != NULL &&
        (outputs.state != app->last_outputs.state || outputs.error != app->last_outputs.error ||
         outputs.feedback_warning != app->last_outputs.feedback_warning ||
         outputs.calibration_completed)) {
        app->platform.log_transition(app->platform.context, &app->last_outputs, &outputs);
    }
    app->last_outputs = outputs;
    return outputs;
}

bool desk_app_set_control_mode(desk_app_t *app, desk_control_mode_t mode)
{
    if (app == NULL || !app->initialized) {
        return false;
    }
    return desk_command_arbiter_set_mode(&app->command_arbiter, mode);
}

bool desk_app_set_motion_inhibited(desk_app_t *app, bool inhibited)
{
    if (app == NULL || !app->initialized) {
        return false;
    }
    if (inhibited) {
        app->motion_inhibited = true;
        app->motion_release_pending = false;
        desk_command_arbiter_cancel_remote(&app->command_arbiter);
        desk_preset_controller_reset(&app->preset_controller);
    } else if (app->motion_inhibited) {
        app->motion_release_pending = true;
    }
    return true;
}

bool desk_app_request_clear_error(desk_app_t *app)
{
    return app != NULL && app->initialized &&
           desk_controller_request_clear_error(&app->controller, &app->last_inputs,
                                               app->last_step_ms);
}

bool desk_app_set_preset_target(desk_app_t *app, size_t slot, int32_t target_um,
                                int32_t maximum_travel_um)
{
    if (app == NULL || !app->initialized ||
        !desk_preset_bank_set(&app->presets, slot, target_um, maximum_travel_um)) {
        return false;
    }
    if (app->preset_controller.active && app->preset_controller.slot == slot) {
        desk_preset_controller_reset(&app->preset_controller);
    }
    return true;
}

bool desk_app_acquire_remote_lease(desk_app_t *app, uint64_t lease_id)
{
    if (app == NULL || !app->initialized) {
        return false;
    }
    return desk_command_arbiter_acquire_remote_lease(&app->command_arbiter, lease_id);
}

bool desk_app_release_remote_lease(desk_app_t *app, uint64_t lease_id)
{
    if (app == NULL || !app->initialized) {
        return false;
    }
    return desk_command_arbiter_release_remote_lease(&app->command_arbiter, lease_id);
}

bool desk_app_submit_remote_command(desk_app_t *app, const desk_remote_command_t *command)
{
    if (app == NULL || !app->initialized || command == NULL) {
        return false;
    }
    return desk_command_arbiter_submit_remote(&app->command_arbiter, command,
                                              app->platform.now_ms(app->platform.context));
}

bool desk_app_get_status(const desk_app_t *app, desk_app_status_t *status)
{
    if (app == NULL || status == NULL || !app->initialized) {
        return false;
    }
    *status = (desk_app_status_t){
        .inputs = app->last_inputs,
        .outputs = app->last_outputs,
        .control_mode = app->command_arbiter.mode,
        .control_source = app->last_control_source,
        .presets = app->presets,
        .remote_control_active = app->remote_control_active,
        .preset_target_reached = app->preset_controller.target_reached,
        .motion_inhibited = app->motion_inhibited,
    };
    return true;
}
