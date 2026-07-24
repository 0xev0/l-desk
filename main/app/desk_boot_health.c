#include "desk_boot_health.h"

#include "motor_axis.h"

bool desk_boot_status_is_safe(const desk_app_status_t *status)
{
    if (status == NULL || !desk_outputs_are_coherent(&status->outputs)) {
        return false;
    }
    for (size_t axis = 0U; axis < DESK_AXIS_COUNT; ++axis) {
        if (status->outputs.motor[axis].direction != MOTOR_COMMAND_STOP ||
            status->outputs.motor[axis].enable || status->outputs.motor[axis].duty_permille != 0U) {
            return false;
        }
    }
    return true;
}
