# Local Network Control

## Startup and Buttonless Provisioning

The control task starts before networking. NVS, HTTPS, or Wi-Fi failure makes
phone control unavailable but does not change local motion handling.

Every production image receives a unique 128-bit administration secret and TLS
certificate/key during the offline build. The generated credential record
contains:

- the administration token;
- the derived WPA2 provisioning password;
- no private signing or TLS key.

No credential is printed on UART. Store the record offline with the device.

With no station credentials, the ESP32 creates `Desk-XXXXXX`. Join it with the
provisioning password, open the HTTPS address covered by the device
certificate, enter the device code, and submit the target SSID/password.
Credentials are bounded, validated, and committed as one versioned encrypted
NVS record.

After ten failed station connections, the protected provisioning AP returns as
a recovery path. Wi-Fi power saving remains disabled because remote hold-to-run
uses a 100 ms heartbeat with a 250 ms expiry.

UP, DOWN, and CALIBRATE inputs have pull-ups. If those optional buttons are not
connected they read released, allowing `SAFE_START` to complete. First boot
defaults to `HYBRID`, so the same device can be commissioned entirely from the
web console.

## HTTPS Trust Models

The firmware accepts HTTPS/WSS only and requires exact
`Origin: https://<Host>` matching for every state-changing request and control
WebSocket.

Choose one deployment:

### Direct Private CA

Issue a long-lived certificate for the desk's stable local DNS name (and, if
needed, provisioning IP) from a private CA trusted by the phone. Keep the CA
private key off the ESP32. The per-device leaf key is embedded only in its
flash-encrypted firmware.

### Reverse Proxy with Let's Encrypt

Use Caddy, Nginx, or an equivalent LAN reverse proxy:

```text
phone -- public CA HTTPS/WSS --> reverse proxy
      -- private CA HTTPS/WSS --> isolated ESP32
```

The proxy owns the public DNS name and renews the Let's Encrypt certificate.
It must preserve `Host`, `Origin`, cookies, binary WebSocket frames, upgrade
headers, and connection close events. Never terminate TLS into plaintext
HTTP, never bypass authentication, and never expose the ESP32 directly to the
Internet.

Baking a short-lived Let's Encrypt certificate into firmware would require
frequent signed firmware updates and is not the supported renewal model.
Let's Encrypt also does not issue `localhost` certificates.

## Authentication

Login exchanges the 128-bit administration token for an `HttpOnly`, `Secure`,
`SameSite=Strict` RAM cookie valid for at most eight hours. At most four
sessions exist. Five failed logins block new attempts for one minute.

Cookie parsing rejects malformed, duplicate, or oversized values. Assets and
API responses use no-store, CSP, frame denial, MIME-sniffing denial,
referrer/permissions restrictions, and same-origin isolation headers.
Sessions disappear on reboot and logout expires the cookie.

## Browser Controller

The embedded responsive console exposes:

- device/API and motion WebSocket status;
- desk state, height, skew, Hall counts, and each endstop;
- hold-to-run UP, DOWN, CALIBRATE, and M1/M2/M3;
- Buttons, Buttons + phone, and Phone input modes;
- fault reset back to `SAFE_START`;
- Wi-Fi setup, diagnostics, and signed OTA;
- cancelable dialogs, time-bounded requests, and visible errors.

Each held motion control opens one authenticated `desk-control-v1` WSS channel.
Binary commands repeat every 100 ms and expire after 250 ms. Pointer release,
cancel, lost capture, page hide, blur, timeout, socket error, or page close
stops and revokes the matching lease. One remote motor-control socket is
allowed.

## API

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/api/v1/status` | Authenticated controller and health snapshot |
| `GET` | `/api/v1/diagnostics` | Bounded reset/error history |
| `POST` | `/api/v1/session` | Exchange device code for a session |
| `DELETE` | `/api/v1/session` | Logout |
| `POST` | `/api/v1/provision` | Apply Wi-Fi credentials while stopped |
| `POST` | `/api/v1/mode` | Persist control mode while stopped |
| `POST` | `/api/v1/error/clear` | Request controller-validated fault reset |
| `POST` | `/api/v1/preset` | Capture the current referenced height |
| `POST` | `/api/v1/update` | Stream signed firmware to inactive OTA slot |
| `GET` | `/api/v1/control` | Authenticated binary WSS upgrade |

The network layer submits only bounded semantic requests. It cannot access
GPIO, PWM, PCNT, motor IDs, enable lines, or live controller internals.

## Future Transports

BLE and MQTT remain optional future transports. They must consume immutable
status and submit the same leased high-level actions through the command queue.
Network loss, broker failure, or radio restart must resolve to STOP without
affecting physical control.
