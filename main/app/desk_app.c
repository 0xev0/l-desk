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
           platform->service_watchdog != NULL && platform->now_ms != NULL &&
           platform->delay_ms != NULL;
}

static void update_clear_error_request(desk_app_t *app,
                                       const desk_inputs_t *inputs,
                                       uint64_t now_ms)
{
    if (app->controller.state != DESK_STATE_ERROR) {
        app->clear_error_timing = false;
        app->clear_error_armed = false;
        return;
    }

    bool unsafe_control = inputs->up_pressed || inputs->down_pressed ||
                          inputs->emergency_active || inputs->hardware_fault;
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

bool desk_app_init(desk_app_t *app,
                   const desk_platform_t *platform,
                   const desk_config_t *config)
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
    desk_inputs_t inputs = desk_input_filter_update(&app->input_filter,
                                                     &raw_inputs,
                                                     filter_elapsed);
    update_clear_error_request(app, &inputs, now_ms);
    desk_outputs_t outputs = desk_controller_tick(&app->controller, &inputs, now_ms);

    bool calibration_completed = app->last_outputs.state == DESK_STATE_CALIBRATING_DOWN &&
                                 outputs.state == DESK_STATE_IDLE &&
                                 outputs.error == DESK_ERROR_NONE;
    if (calibration_completed && !app->platform.zero_hall_counts(app->platform.context)) {
        desk_controller_report_hardware_fault(&app->controller, now_ms);
        outputs = desk_controller_tick(&app->controller, &inputs, now_ms);
    }

    if (!app->platform.apply_outputs(app->platform.context, &outputs)) {
        desk_controller_report_hardware_fault(&app->controller, now_ms);
        outputs = desk_controller_tick(&app->controller, &inputs, now_ms);
        (void)app->platform.apply_outputs(app->platform.context, &outputs);
    } else if (!app->platform.service_watchdog(app->platform.context)) {
        desk_controller_report_hardware_fault(&app->controller, now_ms);
        outputs = desk_controller_tick(&app->controller, &inputs, now_ms);
        (void)app->platform.apply_outputs(app->platform.context, &outputs);
    }

    if (app->platform.log_transition != NULL &&
        (outputs.state != app->last_outputs.state || outputs.error != app->last_outputs.error ||
         outputs.feedback_warning != app->last_outputs.feedback_warning)) {
        app->platform.log_transition(app->platform.context, &app->last_outputs, &outputs);
    }
    app->last_outputs = outputs;
    return outputs;
}
