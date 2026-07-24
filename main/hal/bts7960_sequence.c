#include "bts7960_sequence.h"

#include <stddef.h>

#include "motor_axis.h"

static bool append_operation(bts7960_sequence_t *sequence, bts7960_operation_kind_t kind,
                             uint16_t value)
{
    if (sequence->count >= BTS7960_MAX_SEQUENCE_OPERATIONS) {
        return false;
    }

    sequence->operations[sequence->count] = (bts7960_operation_t){
        .kind = kind,
        .value = value,
    };
    ++sequence->count;
    return true;
}

static bool append_stop(bts7960_sequence_t *sequence)
{
    return append_operation(sequence, BTS7960_OPERATION_SET_ENABLE, 0U) &&
           append_operation(sequence, BTS7960_OPERATION_SET_RPWM, 0U) &&
           append_operation(sequence, BTS7960_OPERATION_SET_LPWM, 0U);
}

bool bts7960_sequence_build(motor_command_t applied_direction, const desk_axis_output_t *command,
                            bts7960_sequence_t *sequence)
{
    if (sequence == NULL || !motor_axis_command_is_valid(command)) {
        return false;
    }

    *sequence = (bts7960_sequence_t){
        .resulting_direction = command->direction,
    };
    if (command->direction == MOTOR_COMMAND_STOP) {
        return append_stop(sequence);
    }

    if (applied_direction != command->direction && !append_stop(sequence)) {
        return false;
    }

    bts7960_operation_kind_t active = command->direction == MOTOR_COMMAND_UP
                                          ? BTS7960_OPERATION_SET_RPWM
                                          : BTS7960_OPERATION_SET_LPWM;
    bts7960_operation_kind_t inactive = command->direction == MOTOR_COMMAND_UP
                                            ? BTS7960_OPERATION_SET_LPWM
                                            : BTS7960_OPERATION_SET_RPWM;
    return append_operation(sequence, inactive, 0U) &&
           append_operation(sequence, active, command->duty_permille) &&
           append_operation(sequence, BTS7960_OPERATION_SET_ENABLE, 1U);
}

bool bts7960_snapshot_matches(const desk_axis_output_t *command, const bts7960_snapshot_t *snapshot,
                              uint32_t maximum_pwm_duty)
{
    if (!motor_axis_command_is_valid(command) || snapshot == NULL || maximum_pwm_duty == 0U) {
        return false;
    }

    if (command->direction == MOTOR_COMMAND_STOP) {
        return !snapshot->enable_active && snapshot->rpwm_duty == 0U && snapshot->lpwm_duty == 0U;
    }

    uint32_t expected_duty = ((uint32_t)command->duty_permille * maximum_pwm_duty) / 1000U;
    if (command->direction == MOTOR_COMMAND_UP) {
        return snapshot->enable_active && snapshot->rpwm_duty == expected_duty &&
               snapshot->lpwm_duty == 0U;
    }
    return snapshot->enable_active && snapshot->rpwm_duty == 0U &&
           snapshot->lpwm_duty == expected_duty;
}
