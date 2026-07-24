#include "desk_boot_health.h"
#include "test_support.h"

static desk_app_status_t stopped_status(desk_state_t state, desk_error_t error)
{
    desk_app_status_t status = {
        .outputs = {
            .state = state,
            .error = error,
        },
    };
    return status;
}

static void stopped_safe_and_fault_states_are_boot_healthy(void)
{
    desk_app_status_t safe = stopped_status(DESK_STATE_SAFE_START, DESK_ERROR_NONE);
    TEST_ASSERT_TRUE(desk_boot_status_is_safe(&safe));
    desk_app_status_t external_fault =
        stopped_status(DESK_STATE_ERROR, DESK_ERROR_EMERGENCY_STOP);
    external_fault.inputs.emergency_active = true;
    TEST_ASSERT_TRUE(desk_boot_status_is_safe(&external_fault));
}

static void incoherent_or_moving_outputs_are_rejected(void)
{
    TEST_ASSERT_FALSE(desk_boot_status_is_safe(NULL));
    desk_app_status_t moving = stopped_status(DESK_STATE_MOVING_UP, DESK_ERROR_NONE);
    moving.outputs.motor[0] = (desk_axis_output_t){
        .direction = MOTOR_COMMAND_UP,
        .duty_permille = 300U,
        .enable = true,
    };
    TEST_ASSERT_FALSE(desk_boot_status_is_safe(&moving));

    desk_app_status_t invalid = stopped_status(DESK_STATE_IDLE, DESK_ERROR_NONE);
    invalid.outputs.motor[1].direction = MOTOR_COMMAND_UP;
    TEST_ASSERT_FALSE(desk_boot_status_is_safe(&invalid));

    desk_app_status_t inconsistent_error =
        stopped_status(DESK_STATE_ERROR, DESK_ERROR_NONE);
    TEST_ASSERT_FALSE(desk_boot_status_is_safe(&inconsistent_error));
}

int main(void)
{
    stopped_safe_and_fault_states_are_boot_healthy();
    incoherent_or_moving_outputs_are_rejected();
    return 0;
}
