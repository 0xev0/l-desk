#!/usr/bin/env python3

import argparse
import json
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs


WEB_ROOT = Path(__file__).resolve().parents[1] / "main" / "web"
SESSION_COOKIE = "desk_session=mock"
PRESETS = [
    {"valid": True, "target_um": 120000},
    {"valid": True, "target_um": 260000},
    {"valid": False, "target_um": 0},
]


def status_payload():
    axes = [
        {
            "position_um": position,
            "hall": count,
            "hall_valid": True,
            "upper": False,
            "lower": False,
            "direction": 0,
            "duty": 0,
            "enabled": False,
        }
        for position, count in ((247800, 4956), (249100, 4982), (248500, 4970))
    ]
    return {
        "firmware": "mock-test",
        "partition": "ota_0",
        "reset_reason": 1,
        "state": "idle",
        "error": "none",
        "mode": "mobile",
        "source": "none",
        "remote_active": False,
        "referenced": True,
        "feedback_warning": False,
        "emergency": False,
        "provisioning": False,
        "wifi_connected": True,
        "preset_target_reached": False,
        "preset_storage_fault": False,
        "presets": PRESETS,
        "axes": axes,
        "diagnostics": {
            "free_heap": 200000,
            "minimum_free_heap": 180000,
            "dropped_logs": 0,
            "control_stack": 2048,
            "logger_stack": 1024,
        },
    }


class DeskMockHandler(BaseHTTPRequestHandler):
    def log_message(self, _format, *_args):
        return

    def send_json(self, status, payload, cookie=None):
        encoded = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Cache-Control", "no-store")
        if cookie is not None:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        self.wfile.write(encoded)

    def authenticated(self):
        return SESSION_COOKIE in self.headers.get("Cookie", "")

    def read_form(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > 256:
            return {}
        return parse_qs(self.rfile.read(length).decode("ascii"), strict_parsing=True)

    def do_GET(self):
        if self.path == "/api/v1/status":
            if not self.authenticated():
                self.send_json(HTTPStatus.UNAUTHORIZED, {"error": "authentication_required"})
            else:
                self.send_json(HTTPStatus.OK, status_payload())
            return
        assets = {
            "/": ("index.html", "text/html; charset=utf-8"),
            "/app.css": ("app.css", "text/css; charset=utf-8"),
            "/app.js": ("app.js", "application/javascript; charset=utf-8"),
        }
        asset = assets.get(self.path)
        if asset is None:
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        content = (WEB_ROOT / asset[0]).read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", asset[1])
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def do_POST(self):
        if self.path == "/api/v1/update":
            if not self.authenticated():
                self.send_json(HTTPStatus.UNAUTHORIZED,
                               {"error": "authentication_required"})
                return
            length = int(self.headers.get("Content-Length", "0"))
            self.rfile.read(length)
            self.send_json(HTTPStatus.OK, {"rebooting": True})
            return
        try:
            form = self.read_form()
        except (UnicodeDecodeError, ValueError):
            self.send_json(HTTPStatus.BAD_REQUEST, {"error": "invalid_request"})
            return
        if self.path == "/api/v1/session":
            token = form.get("token", [""])[0]
            if len(token) != 32:
                self.send_json(HTTPStatus.UNAUTHORIZED, {"error": "invalid_credentials"})
            else:
                self.send_json(HTTPStatus.OK, {"authenticated": True}, SESSION_COOKIE)
            return
        if not self.authenticated():
            self.send_json(HTTPStatus.UNAUTHORIZED, {"error": "authentication_required"})
            return
        if self.path == "/api/v1/preset":
            slot_text = form.get("slot", [""])[0]
            if slot_text not in ("1", "2", "3"):
                self.send_json(HTTPStatus.BAD_REQUEST, {"error": "invalid_preset_slot"})
                return
            PRESETS[int(slot_text) - 1] = {"valid": True, "target_um": 248466}
            self.send_json(HTTPStatus.OK, {"updated": True})
            return
        if self.path == "/api/v1/mode":
            self.send_json(HTTPStatus.OK, {"updated": True})
            return
        self.send_error(HTTPStatus.NOT_FOUND)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    ThreadingHTTPServer(("127.0.0.1", args.port), DeskMockHandler).serve_forever()


if __name__ == "__main__":
    main()
