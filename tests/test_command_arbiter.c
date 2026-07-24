#include "desk_command_arbiter.h"
#include "test_support.h"

static desk_remote_command_t remote_command(uint64_t lease_id, uint32_t sequence,
                                            desk_control_action_t action, uint32_t valid_for_ms)
{
    return (desk_remote_command_t){
        .lease_id = lease_id,
        .sequence = sequence,
        .valid_for_ms = valid_for_ms,
        .action = action,
    };
}

static void each_mode_has_explicit_source_behavior(void)
{
    desk_command_arbiter_t arbiter;
    desk_local_controls_t local = {.up_pressed = true};

    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_LOCAL_ONLY);
    desk_resolved_controls_t resolved = desk_command_arbiter_resolve(&arbiter, &local, 0U);
    TEST_ASSERT_TRUE(resolved.up_pressed);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_LOCAL, resolved.source);
    desk_remote_command_t remote =
        remote_command(1U, 1U, DESK_CONTROL_ACTION_DOWN, DESK_REMOTE_COMMAND_MAX_LEASE_MS);
    TEST_ASSERT_FALSE(desk_command_arbiter_acquire_remote_lease(&arbiter, 1U));
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &remote, 0U));

    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_MOBILE_ONLY);
    resolved = desk_command_arbiter_resolve(&arbiter, &local, 0U);
    TEST_ASSERT_FALSE(resolved.up_pressed);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 1U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 0U));
    resolved = desk_command_arbiter_resolve(&arbiter, &local, 1U);
    TEST_ASSERT_TRUE(resolved.down_pressed);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_REMOTE, resolved.source);

    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 1U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 0U));
    resolved = desk_command_arbiter_resolve(&arbiter, &local, 1U);
    TEST_ASSERT_TRUE(resolved.up_pressed);
    TEST_ASSERT_FALSE(resolved.down_pressed);
    TEST_ASSERT_TRUE(resolved.remote_preempted);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_LOCAL, resolved.source);
}

static void remote_lease_expires_to_stop_at_deadline(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    desk_remote_command_t remote = remote_command(42U, 1U, DESK_CONTROL_ACTION_UP, 250U);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 42U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 1000U));

    desk_local_controls_t local = {0};
    desk_resolved_controls_t resolved = desk_command_arbiter_resolve(&arbiter, &local, 1249U);
    TEST_ASSERT_TRUE(resolved.up_pressed);
    TEST_ASSERT_TRUE(resolved.remote_active);

    resolved = desk_command_arbiter_resolve(&arbiter, &local, 1250U);
    TEST_ASSERT_FALSE(resolved.up_pressed);
    TEST_ASSERT_FALSE(resolved.remote_active);
    TEST_ASSERT_TRUE(resolved.remote_expired);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &remote, 1251U));
}

static void lease_is_exclusive_and_sequence_replay_is_rejected(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    desk_remote_command_t first = remote_command(10U, 7U, DESK_CONTROL_ACTION_UP, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 10U));
    TEST_ASSERT_FALSE(desk_command_arbiter_acquire_remote_lease(&arbiter, 11U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &first, 0U));

    desk_remote_command_t replay = remote_command(10U, 7U, DESK_CONTROL_ACTION_DOWN, 200U);
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &replay, 1U));
    replay.sequence = 6U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &replay, 1U));
    replay.lease_id = 11U;
    replay.sequence = 8U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &replay, 1U));

    desk_remote_command_t next = remote_command(10U, 8U, DESK_CONTROL_ACTION_DOWN, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &next, 2U));
    desk_resolved_controls_t resolved =
        desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, 2U);
    TEST_ASSERT_TRUE(resolved.down_pressed);
}

static void stop_or_mode_change_releases_lease_for_a_new_session(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    desk_remote_command_t command = remote_command(10U, 1U, DESK_CONTROL_ACTION_UP, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 10U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));
    command.sequence = 2U;
    command.action = DESK_CONTROL_ACTION_STOP;
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &command, 1U));

    command = remote_command(99U, 1U, DESK_CONTROL_ACTION_CALIBRATE, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 99U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &command, 2U));
    desk_resolved_controls_t resolved =
        desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, 2U);
    TEST_ASSERT_TRUE(resolved.calibrate_pressed);

    TEST_ASSERT_TRUE(desk_command_arbiter_set_mode(&arbiter, DESK_CONTROL_MODE_MOBILE_ONLY));
    resolved = desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, 3U);
    TEST_ASSERT_FALSE(resolved.remote_active);
    command = remote_command(100U, 1U, DESK_CONTROL_ACTION_DOWN, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 100U));
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &command, 3U));
}

static void invalid_commands_and_deadline_overflow_fail_safely(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(NULL, DESK_CONTROL_MODE_HYBRID);
    desk_command_arbiter_cancel_remote(NULL);
    desk_command_arbiter_init(&arbiter, (desk_control_mode_t)99);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_MODE_LOCAL_ONLY, arbiter.mode);
    TEST_ASSERT_FALSE(desk_command_arbiter_set_mode(NULL, DESK_CONTROL_MODE_HYBRID));
    TEST_ASSERT_FALSE(desk_command_arbiter_set_mode(&arbiter, (desk_control_mode_t)99));
    TEST_ASSERT_FALSE(desk_command_arbiter_acquire_remote_lease(NULL, 1U));
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(NULL, NULL, 0U));
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, NULL, 0U));

    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    TEST_ASSERT_FALSE(desk_command_arbiter_acquire_remote_lease(&arbiter, 0U));
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 1U));
    desk_remote_command_t command = remote_command(0U, 1U, DESK_CONTROL_ACTION_UP, 200U);
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));
    command.lease_id = 1U;
    command.sequence = 0U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));
    command.sequence = 1U;
    command.valid_for_ms = 0U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));
    command.valid_for_ms = DESK_REMOTE_COMMAND_MAX_LEASE_MS + 1U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));
    command.valid_for_ms = 1U;
    command.action = (desk_control_action_t)99;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &command, 0U));

    command.action = DESK_CONTROL_ACTION_UP;
    command.valid_for_ms = 250U;
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &command, UINT64_MAX - 10U));
    desk_resolved_controls_t resolved =
        desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, UINT64_MAX - 1U);
    TEST_ASSERT_TRUE(resolved.up_pressed);
    resolved = desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, UINT64_MAX);
    TEST_ASSERT_TRUE(resolved.remote_expired);

    resolved = desk_command_arbiter_resolve(NULL, NULL, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);
    resolved = desk_command_arbiter_resolve(&arbiter, NULL, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);
    arbiter.mode = (desk_control_mode_t)99;
    resolved = desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);
}

static void unchanged_inputs_are_deterministic_and_both_local_buttons_survive(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    desk_local_controls_t local = {
        .up_pressed = true,
        .down_pressed = true,
    };
    desk_resolved_controls_t first = desk_command_arbiter_resolve(&arbiter, &local, 10U);
    desk_resolved_controls_t second = desk_command_arbiter_resolve(&arbiter, &local, 10U);
    TEST_ASSERT_TRUE(first.up_pressed);
    TEST_ASSERT_TRUE(first.down_pressed);
    TEST_ASSERT_EQUAL_INT(first.source, second.source);
    TEST_ASSERT_EQUAL_INT(first.up_pressed, second.up_pressed);
    TEST_ASSERT_EQUAL_INT(first.down_pressed, second.down_pressed);
}

static void local_preemption_revokes_the_session_and_blocks_old_heartbeats(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 10U));
    desk_remote_command_t remote = remote_command(10U, 1U, DESK_CONTROL_ACTION_UP, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 0U));

    desk_resolved_controls_t resolved = desk_command_arbiter_resolve(
        &arbiter, &(desk_local_controls_t){.down_pressed = true}, 1U);
    TEST_ASSERT_TRUE(resolved.remote_preempted);
    TEST_ASSERT_FALSE(arbiter.remote_lease_authorized);

    remote.sequence = 2U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &remote, 2U));
    resolved = desk_command_arbiter_resolve(&arbiter, &(desk_local_controls_t){0}, 2U);
    TEST_ASSERT_EQUAL_INT(DESK_CONTROL_SOURCE_NONE, resolved.source);

    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 11U));
    remote.lease_id = 11U;
    remote.sequence = 1U;
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 3U));
}

static void command_arriving_after_expiry_cannot_revive_the_lease(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 50U));
    desk_remote_command_t remote = remote_command(50U, 1U, DESK_CONTROL_ACTION_UP, 10U);
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 100U));

    remote.sequence = 2U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &remote, 110U));
    TEST_ASSERT_FALSE(arbiter.remote_lease_authorized);
    TEST_ASSERT_FALSE(arbiter.remote_active);
}

static void only_the_owning_transport_can_release_a_remote_lease(void)
{
    desk_command_arbiter_t arbiter;
    desk_command_arbiter_init(&arbiter, DESK_CONTROL_MODE_HYBRID);
    TEST_ASSERT_TRUE(desk_command_arbiter_acquire_remote_lease(&arbiter, 50U));
    desk_remote_command_t remote = remote_command(50U, 1U, DESK_CONTROL_ACTION_UP, 200U);
    TEST_ASSERT_TRUE(desk_command_arbiter_submit_remote(&arbiter, &remote, 100U));

    TEST_ASSERT_FALSE(desk_command_arbiter_release_remote_lease(NULL, 50U));
    TEST_ASSERT_FALSE(desk_command_arbiter_release_remote_lease(&arbiter, 0U));
    TEST_ASSERT_FALSE(desk_command_arbiter_release_remote_lease(&arbiter, 51U));
    TEST_ASSERT_TRUE(arbiter.remote_active);
    TEST_ASSERT_TRUE(arbiter.remote_lease_authorized);

    TEST_ASSERT_TRUE(desk_command_arbiter_release_remote_lease(&arbiter, 50U));
    TEST_ASSERT_FALSE(arbiter.remote_active);
    TEST_ASSERT_FALSE(arbiter.remote_lease_authorized);
    remote.sequence = 2U;
    TEST_ASSERT_FALSE(desk_command_arbiter_submit_remote(&arbiter, &remote, 101U));
    TEST_ASSERT_FALSE(desk_command_arbiter_release_remote_lease(&arbiter, 50U));
}

int main(void)
{
    each_mode_has_explicit_source_behavior();
    remote_lease_expires_to_stop_at_deadline();
    lease_is_exclusive_and_sequence_replay_is_rejected();
    stop_or_mode_change_releases_lease_for_a_new_session();
    invalid_commands_and_deadline_overflow_fail_safely();
    unchanged_inputs_are_deterministic_and_both_local_buttons_survive();
    local_preemption_revokes_the_session_and_blocks_old_heartbeats();
    command_arriving_after_expiry_cannot_revive_the_lease();
    only_the_owning_transport_can_release_a_remote_lease();
    return 0;
}
