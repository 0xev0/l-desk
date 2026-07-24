#include "bts7960_sequence.h"
#include "test_support.h"

static desk_axis_output_t command(motor_command_t direction, uint16_t duty, bool enable)
{
    return (desk_axis_output_t){
        .direction = direction,
        .duty_permille = duty,
        .enable = enable,
    };
}

static void assert_operation(const bts7960_sequence_t *sequence, size_t index,
                             bts7960_operation_kind_t kind, uint16_t value)
{
    TEST_ASSERT_TRUE(index < sequence->count);
    TEST_ASSERT_EQUAL_INT(kind, sequence->operations[index].kind);
    TEST_ASSERT_EQUAL_INT(value, sequence->operations[index].value);
}

static void stop_always_disables_before_clearing_pwm(void)
{
    desk_axis_output_t stop = command(MOTOR_COMMAND_STOP, 0U, false);
    bts7960_sequence_t sequence;

    TEST_ASSERT_TRUE(bts7960_sequence_build(MOTOR_COMMAND_UP, &stop, &sequence));
    TEST_ASSERT_EQUAL_INT(3, sequence.count);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, sequence.resulting_direction);
    assert_operation(&sequence, 0U, BTS7960_OPERATION_SET_ENABLE, 0U);
    assert_operation(&sequence, 1U, BTS7960_OPERATION_SET_RPWM, 0U);
    assert_operation(&sequence, 2U, BTS7960_OPERATION_SET_LPWM, 0U);
}

static void start_and_reversal_have_safe_ordering(void)
{
    desk_axis_output_t down = command(MOTOR_COMMAND_DOWN, 600U, true);
    bts7960_sequence_t sequence;

    TEST_ASSERT_TRUE(bts7960_sequence_build(MOTOR_COMMAND_STOP, &down, &sequence));
    TEST_ASSERT_EQUAL_INT(BTS7960_MAX_SEQUENCE_OPERATIONS, sequence.count);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, sequence.resulting_direction);
    assert_operation(&sequence, 0U, BTS7960_OPERATION_SET_ENABLE, 0U);
    assert_operation(&sequence, 1U, BTS7960_OPERATION_SET_RPWM, 0U);
    assert_operation(&sequence, 2U, BTS7960_OPERATION_SET_LPWM, 0U);
    assert_operation(&sequence, 3U, BTS7960_OPERATION_SET_RPWM, 0U);
    assert_operation(&sequence, 4U, BTS7960_OPERATION_SET_LPWM, 600U);
    assert_operation(&sequence, 5U, BTS7960_OPERATION_SET_ENABLE, 1U);

    desk_axis_output_t up = command(MOTOR_COMMAND_UP, 400U, true);
    TEST_ASSERT_TRUE(bts7960_sequence_build(MOTOR_COMMAND_DOWN, &up, &sequence));
    TEST_ASSERT_EQUAL_INT(BTS7960_MAX_SEQUENCE_OPERATIONS, sequence.count);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, sequence.resulting_direction);
    assert_operation(&sequence, 0U, BTS7960_OPERATION_SET_ENABLE, 0U);
    assert_operation(&sequence, 4U, BTS7960_OPERATION_SET_RPWM, 400U);
    assert_operation(&sequence, 5U, BTS7960_OPERATION_SET_ENABLE, 1U);
}

static void same_direction_updates_pwm_without_enable_drop(void)
{
    desk_axis_output_t up = command(MOTOR_COMMAND_UP, 750U, true);
    bts7960_sequence_t sequence;

    TEST_ASSERT_TRUE(bts7960_sequence_build(MOTOR_COMMAND_UP, &up, &sequence));
    TEST_ASSERT_EQUAL_INT(3, sequence.count);
    assert_operation(&sequence, 0U, BTS7960_OPERATION_SET_LPWM, 0U);
    assert_operation(&sequence, 1U, BTS7960_OPERATION_SET_RPWM, 750U);
    assert_operation(&sequence, 2U, BTS7960_OPERATION_SET_ENABLE, 1U);
}

static void readback_rejects_every_incoherent_combination(void)
{
    desk_axis_output_t up = command(MOTOR_COMMAND_UP, 500U, true);
    bts7960_snapshot_t snapshot = {
        .enable_active = true,
        .rpwm_duty = 511U,
        .lpwm_duty = 0U,
    };
    TEST_ASSERT_TRUE(bts7960_snapshot_matches(&up, &snapshot, 1023U));
    snapshot.lpwm_duty = 1U;
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&up, &snapshot, 1023U));
    snapshot.lpwm_duty = 0U;
    snapshot.enable_active = false;
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&up, &snapshot, 1023U));
    snapshot.enable_active = true;
    snapshot.rpwm_duty = 510U;
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&up, &snapshot, 1023U));

    desk_axis_output_t down = command(MOTOR_COMMAND_DOWN, 500U, true);
    snapshot = (bts7960_snapshot_t){
        .enable_active = true,
        .rpwm_duty = 0U,
        .lpwm_duty = 511U,
    };
    TEST_ASSERT_TRUE(bts7960_snapshot_matches(&down, &snapshot, 1023U));
    snapshot.rpwm_duty = 1U;
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&down, &snapshot, 1023U));

    desk_axis_output_t stop = command(MOTOR_COMMAND_STOP, 0U, false);
    snapshot = (bts7960_snapshot_t){0};
    TEST_ASSERT_TRUE(bts7960_snapshot_matches(&stop, &snapshot, 1023U));
    snapshot.rpwm_duty = 1U;
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&stop, &snapshot, 1023U));
}

static void invalid_inputs_fail_closed(void)
{
    bts7960_sequence_t sequence;
    desk_axis_output_t invalid = command(MOTOR_COMMAND_UP, 0U, true);
    TEST_ASSERT_FALSE(bts7960_sequence_build(MOTOR_COMMAND_STOP, &invalid, &sequence));
    TEST_ASSERT_FALSE(bts7960_sequence_build(MOTOR_COMMAND_STOP, NULL, &sequence));
    TEST_ASSERT_FALSE(bts7960_sequence_build(MOTOR_COMMAND_STOP, &invalid, NULL));
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(&invalid, &(bts7960_snapshot_t){0}, 1023U));
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(
        &(desk_axis_output_t){
            .direction = MOTOR_COMMAND_STOP,
            .duty_permille = 0U,
            .enable = false,
        },
        NULL, 1023U));
    TEST_ASSERT_FALSE(bts7960_snapshot_matches(
        &(desk_axis_output_t){
            .direction = MOTOR_COMMAND_STOP,
            .duty_permille = 0U,
            .enable = false,
        },
        &(bts7960_snapshot_t){0}, 0U));
}

int main(void)
{
    stop_always_disables_before_clearing_pwm();
    start_and_reversal_have_safe_ordering();
    same_direction_updates_pwm_without_enable_drop();
    readback_rejects_every_incoherent_combination();
    invalid_inputs_fail_closed();
    return 0;
}
