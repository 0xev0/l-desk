# Local Network Control

## Startup and Provisioning

The control task starts before networking. A network, NVS, HTTP, or WiFi
failure is logged but does not stop local safety control.

On first boot, the ESP32 creates a WPA2 SoftAP named `Desk-XXXXXX`. The serial
console prints:

- the generated SoftAP password;
- the 128-bit device administration token.

Open `http://192.168.4.1/`, authenticate with the device token, and submit the
target SSID and password. Credentials are bounded, decoded without heap
allocation, validated, and committed as one versioned NVS blob. The SoftAP
remains available while STA connects and is disabled after the station obtains
an IP address.

After ten failed station connections, the protected SoftAP returns as a
recovery path. WiFi power saving is disabled because remote hold-to-run uses a
100 ms heartbeat with a 250 ms hard expiry.

## Authentication

There is no default administration password. The device creates a random
128-bit token once and stores it in NVS. Login exchanges this token for an
`HttpOnly`, `SameSite=Strict` RAM session cookie valid for at most eight hours.
At most four sessions exist; five failed logins block attempts for one minute.

Mutating HTTP requests and the WebSocket handshake require an exact
`Origin: http://<Host>` match. Cookie parsing rejects malformed, duplicate, or
oversized values. Browser assets use a restrictive Content Security Policy,
frame denial, MIME sniffing denial, and no-store responses.

The current local API is HTTP, not HTTPS. WPA2 protects first-boot SoftAP
traffic, but the home LAN is still a trust boundary: do not expose port 80 to
the Internet or an untrusted VLAN. A hostile LAN client able to sniff traffic
can steal the token/session and issue authenticated requests. Signed OTA
prevents that client from installing an untrusted image, but does not encrypt
control traffic. Put the desk on a trusted WPA2/WPA3 LAN or isolated IoT VLAN.

## Mobile Web Controller

The ESP32 serves a responsive controller at `/`. It supports:

- three-axis status, endstops, reference state, error, and height;
- hold-to-run UP, DOWN, and CALIBRATE;
- capture and hold-to-run M1/M2/M3 presets;
- local, hybrid, and mobile-only modes;
- WiFi provisioning while all motors are stopped;
- signed firmware upload while a maintenance STOP inhibit is active.

Each press opens one authenticated `desk-control-v1` WebSocket. Binary commands
are sent every 100 ms and expire after 250 ms. Pointer release, cancellation,
lost capture, page hide, window blur, network error, or page close sends STOP
where possible and closes the socket. Server disconnect also revokes the
matching lease. The server permits only one remote motor-control WebSocket.

STOP revokes the lease. A later gesture must complete a fresh authenticated
handshake, so packets from a previous gesture cannot resume motion.

## API

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/status` | Authenticated controller and diagnostic snapshot |
| `GET` | `/api/v1/diagnostics` | Bounded newest-first reset/error history |
| `POST` | `/api/v1/session` | Exchange device token for session cookie |
| `DELETE` | `/api/v1/session` | Logout and expire the cookie |
| `POST` | `/api/v1/provision` | Save and apply WiFi credentials while stopped |
| `POST` | `/api/v1/mode` | Persist and apply control source mode while stopped |
| `POST` | `/api/v1/preset` | Capture one safe referenced preset while idle |
| `POST` | `/api/v1/update` | Stream a signed binary to the inactive OTA slot |
| `GET` | `/api/v1/control` | Authenticated binary WebSocket upgrade |

The network layer can submit only high-level actions through the bounded
application queue. It cannot access GPIO, PWM, PCNT, motor IDs, or controller
internals.

## Why WiFi First

SoftAP provisioning already provides a complete credential path from any phone
without a native app, pairing database, or BLE permission flow. The installed
web app works with no buttons in `MOBILE_ONLY`. BLE provisioning therefore
adds code size and another radio protocol without removing a release blocker.
MQTT similarly adds broker, CA, topic, and reconnect configuration; it is
deferred until Home Assistant integration has a concrete requirement.

The extension boundary is ready for both: telemetry consumes immutable status
snapshots, and any future motion transport must use the same authenticated,
leased high-level command queue. Network loss can never bypass the 250 ms STOP.
