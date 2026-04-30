#include "motor_axis.h"

#include <stddef.h>

bool motor_axis_command_is_valid(motor_command_t command)
{
    switch (command) {
    case MOTOR_COMMAND_STOP:
    case MOTOR_COMMAND_UP:
    case MOTOR_COMMAND_DOWN:
        return true;
    default:
        return false;
    }
}

motor_axis_output_t motor_axis_command_to_output(motor_command_t command)
{
    motor_axis_output_t output = {
        .rpwm_active = false,
        .lpwm_active = false,
        .enable_active = false,
    };

    switch (command) {
    case MOTOR_COMMAND_UP:
        output.rpwm_active = true;
        output.enable_active = true;
        break;
    case MOTOR_COMMAND_DOWN:
        output.lpwm_active = true;
        output.enable_active = true;
        break;
    case MOTOR_COMMAND_STOP:
    default:
        break;
    }

    return output;
}

bool motor_axis_output_is_safe(const motor_axis_output_t *output)
{
    if (output == NULL) {
        return false;
    }

    return !(output->rpwm_active && output->lpwm_active);
}

bool motor_axis_output_is_coherent(const motor_axis_output_t *output)
{
    if (!motor_axis_output_is_safe(output)) {
        return false;
    }

    if (!output->rpwm_active && !output->lpwm_active) {
        return !output->enable_active;
    }

    return output->enable_active;
}
