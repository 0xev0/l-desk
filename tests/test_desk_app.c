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
    desk_config_t config = desk_config_default();
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
}

static void read_and_write_failures_fail_stopped(void)
{
    fake_platform_t fake;
    fake_platform_reset(&fake);
    desk_platform_t platform = fake_platform_interface(&fake);
    desk_config_t config = desk_config_default();
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
    desk_config_t config = desk_config_default();
    desk_app_t app;
    TEST_ASSERT_TRUE(desk_app_init(&app, &platform, &config));
    run_steps(&app, &fake, 52U);

    fake.inputs.up_pressed = true;
    fake.inputs.down_pressed = true;
    run_steps(&app, &fake, 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, fake.outputs.state);

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

static void incomplete_platform_is_rejected(void)
{
    desk_app_t app;
    desk_platform_t platform = {0};
    TEST_ASSERT_FALSE(desk_app_init(&app, &platform, NULL));
}

int main(void)
{
    app_runs_through_injected_platform();
    read_and_write_failures_fail_stopped();
    error_clear_requires_long_hold_and_release();
    incomplete_platform_is_rejected();
    return 0;
}
