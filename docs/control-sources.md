# Control Sources

## Operating Modes

The application supports three explicit modes above the safety controller:

| Mode | Physical controls | Authenticated remote controls |
|---|---|---|
| `LOCAL_ONLY` | Used | Rejected |
| `HYBRID` | Used and takes priority | Used while no local control is active |
| `MOBILE_ONLY` | Motion controls ignored | Used |

Limits, Hall feedback, the monitored E-stop, timing checks, and output
validation are never disabled by these modes. `MOBILE_ONLY` permits omitting
UP, DOWN, and CALIBRATE buttons, but it does not permit omitting the physical
E-stop or NC endstops. Unconnected active-low button inputs remain released
through their configured pull-ups.

The first-boot default is `HYBRID`. The selected mode is validated and
persisted in NVS, then applied through the same bounded control queue after
reboot.

## Remote Lease

Remote movement is hold-to-run, not a durable command:

1. An authenticated client obtains a fresh nonzero 64-bit lease ID.
2. Each command carries that lease ID, a strictly increasing sequence number,
   an action, and a validity interval no longer than 250 ms.
3. The client refreshes the command approximately every 100 ms while the user
   continues to hold the UI control.
4. At the exact expiry deadline, all remote control resolves to STOP.
5. Replay, out-of-order sequence, wrong lease, second-client lease, and
   overlong TTL are rejected.

STOP, heartbeat expiry, local preemption, or a mode change revokes the complete
lease. Old packets cannot reacquire it; the authenticated transport must issue
a new lease. The runtime transfers requests to the control task through a
bounded static queue. A full queue rejects the request, while lease expiry
still guarantees STOP.

## Priority

The order is:

1. E-stop, hardware/input faults, limits, Hall faults, and controller timing
2. Local controls in `HYBRID`
3. The one authorized live remote lease
4. STOP

Local UP plus DOWN remains a latched controller error. A local action that
arrives during remote movement revokes the remote lease; direction reversal
still passes through the normal STOP and 300 ms dead time.

Remote calibration and remote error clear use the same CALIBRATE hold/release
gesture as the physical button. This is necessary for a buttonless
installation, but remains subject to authentication, heartbeat expiry, healthy
safety inputs, and the domain state machine. The transport never submits raw
GPIO, PWM, enable, or per-axis commands.

## Transport Boundary

The command arbiter and its tests do not depend on WiFi, BLE, HTTP, WebSocket,
JSON, or ESP-IDF. The current HTTP/WebSocket transport may only enqueue:

- acquire remote lease;
- high-level UP, DOWN, CALIBRATE, or STOP;
- high-level hold-to-run M1, M2, or M3;
- select an allowed control mode.

Network parsing, authentication, provisioning, rate limits, and client
ownership remain outside the control task. The ESP-IDF WiFi backend runs as a
station when credentials are valid. With no credentials it starts a
password-protected provisioning SoftAP; after ten failed station connections
it enables the same SoftAP as a bounded recovery path. WiFi power saving is
disabled so a 100 ms command heartbeat is not delayed unpredictably.

Authentication uses a device-generated 128-bit administration secret, never a
default password. A successful login creates one of four fixed RAM sessions
with an eight-hour maximum lifetime. Secrets use bounded hexadecimal parsing
and constant-time comparison. Five failed logins block new attempts for one
minute. Sessions disappear on reboot and can be logged out explicitly.

WiFi SSID and PSK are validated with fixed maximum lengths and persisted as one
versioned NVS blob, so a power loss cannot commit half of a credential pair.
The selected control mode and three presets are stored separately. Presets use
a versioned fixed-size record with CRC and bounds checks. NVS encryption is not
enabled; physical flash access can recover credentials and tokens.

## WebSocket Command Frame

Movement uses a fixed 12-byte binary frame. Multi-byte values are big-endian.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Protocol version, currently `1` |
| 1 | 1 | Message type, `1` for command |
| 2 | 1 | `0 STOP`, `1 UP`, `2 DOWN`, `3 CALIBRATE`, `4..6 M1..M3` |
| 3 | 1 | Reserved, must be zero |
| 4 | 4 | Nonzero monotonically increasing sequence |
| 8 | 2 | Validity in milliseconds, `1..250` |
| 10 | 2 | Reserved, must be zero |

The server binds the frame to the authenticated WebSocket lease; the client
does not choose or transmit motor IDs. Wrong size, version, type, action,
sequence, TTL, or reserved bits is rejected before anything reaches the
application queue.

Remote movement is exposed only through the authenticated HTTP/WebSocket
service. Session ownership, same-origin checks, one-client exclusivity,
disconnect revocation, and the mobile client all sit above the unchanged
safety controller.
