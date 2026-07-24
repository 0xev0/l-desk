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
the Internet or an untrusted VLAN. TLS, certificate lifecycle, encrypted NVS,
and signed OTA belong to the P4 security gate.

## Mobile Web Controller

The ESP32 serves a responsive controller at `/`. It supports:

- three-axis status, endstops, reference state, error, and height;
- hold-to-run UP, DOWN, and CALIBRATE;
- local, hybrid, and mobile-only modes;
- WiFi provisioning while all motors are stopped.

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
| `POST` | `/api/v1/session` | Exchange device token for session cookie |
| `DELETE` | `/api/v1/session` | Logout and expire the cookie |
| `POST` | `/api/v1/provision` | Save and apply WiFi credentials while stopped |
| `POST` | `/api/v1/mode` | Persist and apply control source mode while stopped |
| `GET` | `/api/v1/control` | Authenticated binary WebSocket upgrade |

The network layer can submit only high-level actions through the bounded
application queue. It cannot access GPIO, PWM, PCNT, motor IDs, or controller
internals.
