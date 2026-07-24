#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_types.h"

#define BTS7960_MAX_SEQUENCE_OPERATIONS 6U

typedef enum {
    BTS7960_OPERATION_SET_ENABLE = 0,
    BTS7960_OPERATION_SET_RPWM,
    BTS7960_OPERATION_SET_LPWM,
} bts7960_operation_kind_t;

typedef struct {
    bts7960_operation_kind_t kind;
    uint16_t value;
} bts7960_operation_t;

typedef struct {
    bts7960_operation_t operations[BTS7960_MAX_SEQUENCE_OPERATIONS];
    size_t count;
    motor_command_t resulting_direction;
} bts7960_sequence_t;

typedef struct {
    bool enable_active;
    uint32_t rpwm_duty;
    uint32_t lpwm_duty;
} bts7960_snapshot_t;

bool bts7960_sequence_build(motor_command_t applied_direction,
                            const desk_axis_output_t *command, bts7960_sequence_t *sequence);
bool bts7960_snapshot_matches(const desk_axis_output_t *command,
                              const bts7960_snapshot_t *snapshot, uint32_t maximum_pwm_duty);
