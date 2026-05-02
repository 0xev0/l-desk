#include "desk_feedback_observer.h"
#include "fake_feedback.h"
#include "feedback_none_source.h"
#include "test_support.h"

static desk_feedback_config_t test_config(void)
{
    return (desk_feedback_config_t){
        .max_axis_delta_units = 10,
        .max_position_step_units = 20,
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

static void observer_reports_ok_from_valid_source(void)
{
    fake_feedback_t fake;
    fake_feedback_reset(&fake);
    fake_feedback_set_inputs(&fake, valid_feedback(100, 105));

    desk_feedback_observer_t observer;
    desk_feedback_observer_init(&observer, fake_feedback_source(&fake), test_config());

    desk_feedback_diagnostics_t diagnostics = desk_feedback_observer_poll(&observer);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_OK, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_NONE, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_ok);
}

static void observer_propagates_missing_source_feedback(void)
{
    fake_feedback_t fake;
    fake_feedback_reset(&fake);

    desk_feedback_observer_t observer;
    desk_feedback_observer_init(&observer, fake_feedback_source(&fake), test_config());

    desk_feedback_diagnostics_t diagnostics = desk_feedback_observer_poll(&observer);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_MISSING, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_MISSING, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_missing);
}

static void observer_uses_monitor_for_position_jump_faults(void)
{
    fake_feedback_t fake;
    fake_feedback_reset(&fake);
    fake_feedback_set_inputs(&fake, valid_feedback(100, 100));

    desk_feedback_observer_t observer;
    desk_feedback_observer_init(&observer, fake_feedback_source(&fake), test_config());
    (void)desk_feedback_observer_poll(&observer);

    fake_feedback_set_inputs(&fake, valid_feedback(121, 121));
    desk_feedback_diagnostics_t diagnostics = desk_feedback_observer_poll(&observer);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_POSITION_JUMP, diagnostics.error);
    TEST_ASSERT_TRUE(diagnostics.feedback_fault);
}

static void observer_rejects_missing_read_function(void)
{
    desk_feedback_source_t source = {
        .read = NULL,
        .context = NULL,
    };

    desk_feedback_observer_t observer;
    desk_feedback_observer_init(&observer, source, test_config());

    desk_feedback_diagnostics_t diagnostics = desk_feedback_observer_poll(&observer);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_INVALID_INPUT, diagnostics.error);
}

static void null_observer_poll_reports_invalid_input(void)
{
    desk_feedback_diagnostics_t diagnostics = desk_feedback_observer_poll(NULL);

    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_DIAGNOSTIC_FAULT, diagnostics.status);
    TEST_ASSERT_EQUAL_INT(DESK_FEEDBACK_ERROR_INVALID_INPUT, diagnostics.error);
}

static void no_feedback_source_reports_missing_feedback(void)
{
    desk_feedback_source_t source = feedback_none_source();
    desk_feedback_inputs_t inputs = source.read(source.context);

    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_MISSING, inputs.left.status);
    TEST_ASSERT_EQUAL_INT(DESK_AXIS_FEEDBACK_MISSING, inputs.right.status);
}

static void diagnostics_change_detector_checks_status_and_error(void)
{
    desk_feedback_diagnostics_t previous = {
        .status = DESK_FEEDBACK_DIAGNOSTIC_OK,
        .error = DESK_FEEDBACK_ERROR_NONE,
    };
    desk_feedback_diagnostics_t same = previous;
    desk_feedback_diagnostics_t status_changed = {
        .status = DESK_FEEDBACK_DIAGNOSTIC_MISSING,
        .error = DESK_FEEDBACK_ERROR_NONE,
    };
    desk_feedback_diagnostics_t error_changed = {
        .status = DESK_FEEDBACK_DIAGNOSTIC_OK,
        .error = DESK_FEEDBACK_ERROR_POSITION_JUMP,
    };

    TEST_ASSERT_FALSE(desk_feedback_diagnostics_changed(&previous, &same));
    TEST_ASSERT_TRUE(desk_feedback_diagnostics_changed(&previous, &status_changed));
    TEST_ASSERT_TRUE(desk_feedback_diagnostics_changed(&previous, &error_changed));
    TEST_ASSERT_TRUE(desk_feedback_diagnostics_changed(NULL, &same));
    TEST_ASSERT_TRUE(desk_feedback_diagnostics_changed(&previous, NULL));
}

int main(void)
{
    observer_reports_ok_from_valid_source();
    observer_propagates_missing_source_feedback();
    observer_uses_monitor_for_position_jump_faults();
    observer_rejects_missing_read_function();
    null_observer_poll_reports_invalid_input();
    no_feedback_source_reports_missing_feedback();
    diagnostics_change_detector_checks_status_and_error();

    return 0;
}
