# Phase Roadmap

Phase 1 is the only implemented production scope. Later phases must preserve the existing safety controller as the final authority for motor commands.

## Phase 1: Safe Manual Controller

Status: implemented and verified by host tests plus ESP-IDF build.

Scope:

- Manual hold-to-run UP/DOWN
- Calibration down to lower endstops
- NC limit and emergency stop handling
- Direction-change dead time
- Alignment timeout
- App-layer safety input filtering
- Strict warning builds

## Phase 2: Feedback Instrumentation

Do not implement until hardware is selected.

Detailed requirements live in `docs/phase-2-feedback-requirements.md`.

Candidate scope:

- Add position or synchronization sensors through a new HAL interface.
- Record measured actuator drift without automatic correction at first.
- Add diagnostics for sensor plausibility and wiring faults.
- Keep manual movement as the default operating mode.

Required before implementation:

- Exact sensor type, electrical interface, and pin budget.
- Safe behavior for sensor disagreement.
- Host tests for every sensor fault mode.

## Phase 3: Assisted Positioning

Do not implement until Phase 2 feedback is reliable.

Candidate scope:

- Controlled movement to explicit target heights.
- Preset height memory.
- Soft limits derived from calibrated travel.

Safety constraints:

- No unattended movement without explicit command policy.
- Emergency, limits, dead-time, and alignment faults must override all targets.
- Positioning must fail stopped when feedback is unavailable or inconsistent.

## Phase 4: User Interface And Connectivity

Do not implement until the motion controller is proven with feedback.

Candidate scope:

- Display or local UI.
- WiFi, Home Assistant, or app integration.
- Persistent diagnostics.

Safety constraints:

- Remote or UI commands must enter through the same domain command API.
- Connectivity must not directly command GPIO or bypass safety state.
- Network failure must not affect local emergency stop or manual controls.
