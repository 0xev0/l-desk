#include "desk_feedback.h"
#include "fake_feedback.h"
#include "test_support.h"

static desk_feedback_config_t test_config(void)
{
    return (desk_feedback_config_t){
        .max_axis_delta_units = 10,
    };
}

static desk_feedback_inputs_t valid_feedback(int32_t left_position, int32_t right_position)
{
    return (desk_feedback_inputs_t){
        .left = {
            .status = DESK_AXIS_FEEDBACK_VALID,
            .position_units = left_position,
        },
        .right = {
            .status = DESK_AXIS_FEEDBACK_VALID,
            .position_units = right_position,
        },
    };
}

static void both_axes_valid_reports_ok(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 105);

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_OK, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_NONE, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_ok);
    TEST_ASSERT_FALSE(diagnostics.feedback_missing);
    TEST_ASSERT_FALSE(diagnostics.feedback_fault);
}

static void missing_axis_reports_missing(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 100);
    inputs.right.status = DESK_AXIS_FEEDBACK_MISSING;

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_MISSING, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_MISSING, diagnostics.error);
    TEST_ASSERT_FALSE(diagnostics.feedback_ok);
    TEST_ASSERT_TRUE(diagnostics.feedback_missing);
    TEST_ASSERT_FALSE(diagnostics.feedback_fault);
}

static void faulted_axis_reports_fault(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 100);
    inputs.left.status = DESK_AXIS_FEEDBACK_FAULT;

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_AXIS_FAULT, diagnostics.error);
    TEST_ASSERT_FALSE(diagnostics.feedback_ok);
    TEST_ASSERT_FALSE(diagnostics.feedback_missing);
    TEST_ASSERT_TRUE(diagnostics.feedback_fault);
}

static void fault_takes_priority_over_missing(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 100);
    inputs.left.status = DESK_AXIS_FEEDBACK_MISSING;
    inputs.right.status = DESK_AXIS_FEEDBACK_FAULT;

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_AXIS_FAULT, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_fault);
}

static void axis_disagreement_reports_fault(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 111);

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_AXIS_DISAGREEMENT, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_fault);
}

static void disagreement_at_threshold_is_ok(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 110);

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(&config, &inputs);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_OK, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_NONE, diagnostics.error);
}

static void null_arguments_report_fault(void)
{
    desk_feedback_config_t config = test_config();
    desk_feedback_inputs_t inputs = valid_feedback(100, 100);

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(NULL, &inputs);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_INVALID_INPUT, diagnostics.error);

    diagnostics = desk_feedback_evaluate(&config, NULL);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_INVALID_INPUT, diagnostics.error);
}

static void fake_feedback_provider_returns_configured_inputs(void)
{
    fake_feedback_t fake;
    fake_feedback_reset(&fake);

    desk_feedback_inputs_t inputs = valid_feedback(42, 45);
    fake_feedback_set_inputs(&fake, inputs);

    desk_feedback_source_t source = fake_feedback_source(&fake);
    desk_feedback_inputs_t read_inputs = source.read(source.context);

    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_VALID, read_inputs.left.status);
    TEST_ASSERT_EQUAL_INT(42, read_inputs.left.position_units);
    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_VALID, read_inputs.right.status);
    TEST_ASSERT_EQUAL_INT(45, read_inputs.right.position_units);
}

static void fake_feedback_null_context_reads_missing(void)
{
    desk_feedback_source_t source = fake_feedback_source(NULL);
    desk_feedback_inputs_t read_inputs = source.read(source.context);

    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_MISSING, read_inputs.left.status);
    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_MISSING, read_inputs.right.status);
}

int main(void)
{
    both_axes_valid_reports_ok();
    missing_axis_reports_missing();
    faulted_axis_reports_fault();
    fault_takes_priority_over_missing();
    axis_disagreement_reports_fault();
    disagreement_at_threshold_is_ok();
    null_arguments_report_fault();
    fake_feedback_provider_returns_configured_inputs();
    fake_feedback_null_context_reads_missing();

    return 0;
}
