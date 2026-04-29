# Electrical Design

The physical design uses a 12 V 20/30 A supply with a general fuse and individual fused branches for each actuator.

Each actuator is driven by one BTS7960 H-bridge module. Logic ground must be common between the ESP32-S3, driver inputs, and the 12 V system ground. Motor current must not be carried through breadboards or Dupont jumpers.

## Limit Switches

The four limit switches are normally closed and connected to ground in the healthy, not-triggered state. ESP32 inputs use pull-ups:

```text
LOW  = closed, healthy, not triggered
HIGH = open, limit reached, or wiring fault
```

The firmware treats HIGH as active and stops movement toward that limit.

## Emergency Stop

The emergency stop may physically interrupt motor power. The firmware also monitors an emergency input and stops all commanded motor outputs when active.

## BTS7960 Output Semantics

For each motor:

```text
STOP: RPWM inactive, LPWM inactive
UP:   RPWM active,   LPWM inactive
DOWN: RPWM inactive, LPWM active
```

The platform driver must fail safe to STOP for invalid commands.
