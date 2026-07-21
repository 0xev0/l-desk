#include "motor_axis.h"

#include <stddef.h>

bool motor_axis_command_is_valid(const desk_axis_output_t *command)
{
    if (command == NULL || command->duty_permille > 1000U) {
        return false;
    }

    if (command->direction == MOTOR_COMMAND_STOP) {
        return !command->enable && command->duty_permille == 0U;
    }

    return (command->direction == MOTOR_COMMAND_UP || command->direction == MOTOR_COMMAND_DOWN) &&
           command->enable && command->duty_permille > 0U;
}

motor_axis_output_t motor_axis_command_to_output(const desk_axis_output_t *command)
{
    motor_axis_output_t output = {0};

    if (!motor_axis_command_is_valid(command)) {
        return output;
    }

    output.enable_active = command->enable;
    output.duty_permille = command->duty_permille;
    output.rpwm_active = command->direction == MOTOR_COMMAND_UP;
    output.lpwm_active = command->direction == MOTOR_COMMAND_DOWN;
    return output;
}

bool motor_axis_output_is_safe(const motor_axis_output_t *output)
{
    return output != NULL && !(output->rpwm_active && output->lpwm_active) &&
           output->duty_permille <= 1000U;
}

bool motor_axis_output_is_coherent(const motor_axis_output_t *output)
{
    if (!motor_axis_output_is_safe(output)) {
        return false;
    }

    if (!output->rpwm_active && !output->lpwm_active) {
        return !output->enable_active && output->duty_permille == 0U;
    }

    return output->enable_active && output->duty_permille > 0U;
}
