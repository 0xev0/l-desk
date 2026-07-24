#include "desk_app.h"
#include "fake_platform.h"
#include "test_support.h"

static void run_steps(desk_app_t *app, fake_platform_t *fake, unsigned count)
{
    for (unsigned step = 0; step < count; ++step) {
        fake->now_ms += 10U;
        (void)desk_app_step(app);
    }
}

static void run_remote_hold(desk_app_t *app, fake_platform_t *fake, uint64_t lease_id,
                            uint32_t *sequence, desk_control_action_t action, unsigned ticks)
{
    for (unsigned tick = 0U; tick < ticks; ++tick) {
        if (tick % 10U == 0U) {
            ++*sequence;
            TEST_ASSERT_TRUE(desk_app_submit_remote_command(
                app,
                &(desk_remote_command_t){
                    .lease_id = lease_id,
                    .sequence = *sequence,
                    .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
                    .action = action,
                }));
        }
        run_steps(app, fake, 1U);
    }
}

static desk_config_t app_test_config(void)
{
    desk_config_t config = desk_config_default();
    config.require_position_reference = false;
    return config;
}

static void assert_outputs_stopped(desk_outputs_t outputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.motor[axis].direction);
        TEST_ASSERT_EQUAL_INT(0, outputs.motor[axis].duty_permille);
        TEST_ASSERT_FALSE(outputs.motor[axis].enable);
    }
}

static void app_runs_through_injected_platform(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);

    fake.inputs.up_pressed = true;
    run_steps(&app, &fake, 6U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, fake.outputs.state);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_TRUE(fake.outputs.motor[axis].enable);
    }
    TEST_ASSERT_TRUE(fake.log_calls > 0U);

    desk_app_status_t status;
    TEST_ASSERT_TRUE(desk_app_get_status(&app, &status));
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, status.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_MODE_HYBRID, status.control_mode);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_LOCAL, status.control_source);
    TEST_ASSERT_TRUE(status.inputs.up_pressed);
}

static void read_and_write_failures_fail_stopped(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    fake.fail_read = true;
    fake.now_ms += 10U;
    desk_outputs_t outputs = desk_app_step(&app);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);

    fake_platform_reset(&fake);
    platform = fake_platform_interface(&fake);
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    fake.fail_apply = true;
    fake.now_ms += 10U;
    outputs = desk_app_step(&app);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);

    fake_platform_reset(&fake);
    platform = fake_platform_interface(&fake);
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    fake.fail_watchdog = true;
    fake.now_ms += 10U;
    outputs = desk_app_step(&app);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);
}

static void error_clear_requires_long_hold_and_release(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    fake.inputs.up_pressed = true;
    fake.inputs.down_pressed = true;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, fake.outputs.state);

    fake.inputs.up_pressed = true;
    run_steps(&app, &fake, 1U);
    fake.inputs.up_pressed = false;

    fake.inputs.up_pressed = false;
    fake.inputs.down_pressed = false;
    fake.inputs.calibrate_pressed = true;
    run_steps(&app, &fake, 50U);
    fake.inputs.calibrate_pressed = false;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, fake.outputs.state);

    fake.inputs.calibrate_pressed = true;
    run_steps(&app, &fake, 201U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, fake.outputs.state);
    fake.inputs.calibrate_pressed = false;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_SAFE_START, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);
}

static void calibration_zero_failure_enters_hardware_error(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    fake.inputs.calibrate_pressed = true;
    run_steps(&app, &fake, 201U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_CALIBRATING_DOWN, fake.outputs.state);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        fake.inputs.axis[axis].lower_limit_active = true;
    }
    fake.fail_zero = true;
    run_steps(&app, &fake, 1U);

    TEST_ASSERT_EQUAL_INT(1, fake.zero_calls);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, fake.outputs.error);
    assert_outputs_stopped(fake.outputs);
}

static void calibration_at_lower_limits_zeroes_hall_once(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        fake.inputs.axis[axis].lower_limit_active = true;
    }
    fake.inputs.calibrate_pressed = true;
    run_steps(&app, &fake, 201U);

    TEST_ASSERT_EQUAL_INT(1, fake.zero_calls);
    TEST_ASSERT_TRUE(fake.outputs.calibration_completed);
    assert_outputs_stopped(fake.outputs);
    run_steps(&app, &fake, 201U);
    TEST_ASSERT_EQUAL_INT(1, fake.zero_calls);
}

static void incomplete_platform_is_rejected(void)
{
    desk_app_t app;
    desk_platform_t platform = {0};
    TEST_ASSERT_FALSE(desk_app_init(&app, &platform, NULL));
    desk_outputs_t outputs = desk_app_step(NULL);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);

    fake_platform_t fake;
    fake_platform_reset(&fake);
    platform = fake_platform_interface(&fake);
    fake.fail_initialize = true;
    TEST_ASSERT_FALSE(desk_app_init(&app, &platform, NULL));

    TEST_ASSERT_FALSE(desk_app_set_control_mode(NULL, DESK_CONTROL_MODE_HYBRID));
    TEST_ASSERT_FALSE(desk_app_acquire_remote_lease(NULL, 1U));
    TEST_ASSERT_FALSE(desk_app_release_remote_lease(NULL, 1U));
    TEST_ASSERT_FALSE(desk_app_submit_remote_command(NULL, NULL));
    app = (desk_app_t){0};
    TEST_ASSERT_FALSE(desk_app_set_control_mode(&app, DESK_CONTROL_MODE_HYBRID));
    TEST_ASSERT_FALSE(desk_app_acquire_remote_lease(&app, 1U));
    TEST_ASSERT_FALSE(desk_app_release_remote_lease(&app, 1U));
    TEST_ASSERT_FALSE(desk_app_submit_remote_command(&app, NULL));
    TEST_ASSERT_FALSE(desk_app_get_status(NULL, NULL));
    TEST_ASSERT_FALSE(desk_app_get_status(&app, NULL));
    TEST_ASSERT_FALSE(desk_app_get_status(&app, &(desk_app_status_t){0}));
}

static void mobile_only_ignores_buttons_and_requires_a_live_remote_lease(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    TEST_ASSERT_TRUE(desk_app_set_control_mode(&app, DESK_CONTROL_MODE_MOBILE_ONLY));
    run_steps(&app, &fake, 52U);

    fake.inputs.up_pressed = true;
    run_steps(&app, &fake, 6U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);

    desk_remote_command_t remote = {
        .lease_id = 1234U,
        .sequence = 1U,
        .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
        .action = DESK_CONTROL_ACTION_UP,
    };
    TEST_ASSERT_TRUE(desk_app_acquire_remote_lease(&app, remote.lease_id));
    TEST_ASSERT_TRUE(desk_app_submit_remote_command(&app, &remote));
    run_steps(&app, &fake, 6U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, fake.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_REMOTE, app.last_control_source);

    run_steps(&app, &fake, 19U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);
    TEST_ASSERT_FALSE(app.remote_control_active);
    assert_outputs_stopped(fake.outputs);
}

static void local_control_preempts_remote_in_hybrid_mode(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    config.button_debounce_ms = 0U;
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    desk_remote_command_t remote = {
        .lease_id = 55U,
        .sequence = 1U,
        .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
        .action = DESK_CONTROL_ACTION_UP,
    };
    TEST_ASSERT_TRUE(desk_app_acquire_remote_lease(&app, remote.lease_id));
    TEST_ASSERT_TRUE(desk_app_submit_remote_command(&app, &remote));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, fake.outputs.state);

    fake.inputs.down_pressed = true;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_LOCAL, app.last_control_source);
    TEST_ASSERT_FALSE(app.command_arbiter.remote_active);
    TEST_ASSERT_FALSE(app.command_arbiter.remote_lease_authorized);
    assert_outputs_stopped(fake.outputs);

    fake.inputs.down_pressed = false;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);
}

static void mode_changes_cancel_remote_motion_and_local_only_rejects_network(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    config.button_debounce_ms = 0U;
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    desk_remote_command_t remote = {
        .lease_id = 77U,
        .sequence = 1U,
        .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
        .action = DESK_CONTROL_ACTION_UP,
    };
    TEST_ASSERT_TRUE(desk_app_acquire_remote_lease(&app, remote.lease_id));
    TEST_ASSERT_TRUE(desk_app_submit_remote_command(&app, &remote));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, fake.outputs.state);

    TEST_ASSERT_TRUE(desk_app_set_control_mode(&app, DESK_CONTROL_MODE_LOCAL_ONLY));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);
    remote.lease_id = 78U;
    TEST_ASSERT_FALSE(desk_app_submit_remote_command(&app, &remote));
}

static void transport_disconnect_stops_only_its_active_lease(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    desk_remote_command_t remote = {
        .lease_id = 88U,
        .sequence = 1U,
        .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
        .action = DESK_CONTROL_ACTION_DOWN,
    };
    TEST_ASSERT_TRUE(desk_app_acquire_remote_lease(&app, remote.lease_id));
    TEST_ASSERT_TRUE(desk_app_submit_remote_command(&app, &remote));
    run_steps(&app, &fake, 6U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, fake.outputs.state);

    TEST_ASSERT_FALSE(desk_app_release_remote_lease(&app, 87U));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, fake.outputs.state);

    TEST_ASSERT_TRUE(desk_app_release_remote_lease(&app, remote.lease_id));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);
}

static void mobile_only_can_clear_an_error_without_physical_buttons(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = app_test_config();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    TEST_ASSERT_TRUE(desk_app_set_control_mode(&app, DESK_CONTROL_MODE_MOBILE_ONLY));
    run_steps(&app, &fake, 52U);

    fake.inputs.emergency_active = true;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, fake.outputs.error);
    fake.inputs.emergency_active = false;
    run_steps(&app, &fake, 6U);

    const uint64_t lease_id = 9001U;
    uint32_t sequence = 0U;
    TEST_ASSERT_TRUE(desk_app_acquire_remote_lease(&app, lease_id));
    run_remote_hold(&app, &fake, lease_id, &sequence, DESK_CONTROL_ACTION_CALIBRATE, 201U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, fake.outputs.state);

    ++sequence;
    TEST_ASSERT_TRUE(desk_app_submit_remote_command(
        &app,
        &(desk_remote_command_t){
            .lease_id = lease_id,
            .sequence = sequence,
            .valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS,
            .action = DESK_CONTROL_ACTION_STOP,
        }));
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_SAFE_START, fake.outputs.state);
    assert_outputs_stopped(fake.outputs);
}

int main(void)
{
    app_runs_through_injected_platform();
    read_and_write_failures_fail_stopped();
    error_clear_requires_long_hold_and_release();
    calibration_zero_failure_enters_hardware_error();
    calibration_at_lower_limits_zeroes_hall_once();
    incomplete_platform_is_rejected();
    mobile_only_ignores_buttons_and_requires_a_live_remote_lease();
    local_control_preempts_remote_in_hybrid_mode();
    mode_changes_cancel_remote_motion_and_local_only_rejects_network();
    transport_disconnect_stops_only_its_active_lease();
    mobile_only_can_clear_an_error_without_physical_buttons();
    return 0;
}
