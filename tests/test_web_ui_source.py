#!/usr/bin/env python3

"""Static safety and accessibility contracts for the embedded web controller."""

from html.parser import HTMLParser
from pathlib import Path
import unittest


WEB_ROOT = Path(__file__).resolve().parents[1] / "main" / "web"
HTTP_API = (
    Path(__file__).resolve().parents[1] / "main" / "platform" / "esp_http_api.c"
)


class UiParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = []
        self.html_language = None
        self.dialogs = set()
        self.dialog_close_targets = set()

    def handle_starttag(self, tag, attributes):
        values = dict(attributes)
        if tag == "html":
            self.html_language = values.get("lang")
        if "id" in values:
            self.ids.append(values["id"])
        if tag == "dialog" and "id" in values:
            self.dialogs.add(values["id"])
        if "data-close-dialog" in values:
            self.dialog_close_targets.add(values["data-close-dialog"])


class WebUiSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = (WEB_ROOT / "index.html").read_text(encoding="utf-8")
        cls.css = (WEB_ROOT / "app.css").read_text(encoding="utf-8")
        cls.javascript = (WEB_ROOT / "app.js").read_text(encoding="utf-8")
        cls.http_api = HTTP_API.read_text(encoding="utf-8")
        cls.parser = UiParser()
        cls.parser.feed(cls.html)

    def test_ids_are_unique_and_required_controls_exist(self):
        self.assertEqual(len(self.parser.ids), len(set(self.parser.ids)))
        required = {
            "api-status",
            "socket-status",
            "last-update",
            "move-up",
            "move-down",
            "calibrate",
            "clear-error",
            "login-dialog",
            "confirm-dialog",
        }
        self.assertTrue(required.issubset(self.parser.ids))

    def test_every_dialog_has_an_explicit_escape_path(self):
        self.assertEqual(self.parser.html_language, "en")
        self.assertEqual(self.parser.dialogs, {"login-dialog", "confirm-dialog"})
        self.assertIn("login-dialog", self.parser.dialog_close_targets)
        self.assertIn("data-confirm=\"cancel\"", self.html)
        self.assertIn('loginDialog.addEventListener("cancel"', self.javascript)
        self.assertIn('confirmDialog.addEventListener("cancel"', self.javascript)

    def test_remote_motion_is_https_and_wss_only(self):
        self.assertIn('location.protocol !== "https:"', self.javascript)
        self.assertIn("`wss://${location.host}/api/v1/control`", self.javascript)
        self.assertNotIn('location.protocol === "https:" ? "wss" : "ws"', self.javascript)
        self.assertNotIn("`ws://${", self.javascript)

    def test_browser_security_policy_is_same_origin_and_has_no_unsafe_script_mode(self):
        self.assertIn("connect-src 'self'", self.http_api)
        self.assertIn("script-src 'self'", self.http_api)
        self.assertIn("frame-ancestors 'none'", self.http_api)
        self.assertNotIn("connect-src 'self' wss:", self.http_api)
        self.assertNotIn("unsafe-inline", self.http_api)
        self.assertNotIn("unsafe-eval", self.http_api)
        self.assertNotIn("localStorage", self.javascript)
        self.assertNotIn("sessionStorage", self.javascript)

    def test_remote_motion_has_release_and_transport_fail_safes(self):
        for event in (
            "pointerup",
            "pointercancel",
            "lostpointercapture",
            "blur",
            "pagehide",
            "visibilitychange",
        ):
            self.assertIn(event, self.javascript)
        self.assertIn("SOCKET_CONNECT_TIMEOUT_MS", self.javascript)
        self.assertIn("HEARTBEAT_MS", self.javascript)
        self.assertIn("sendCommand(0)", self.javascript)

    def test_layout_defines_mobile_and_reduced_motion_modes(self):
        self.assertIn("@media (max-width: 440px)", self.css)
        self.assertIn("@media (prefers-reduced-motion: reduce)", self.css)
        self.assertIn("overflow-x: auto", self.css)


if __name__ == "__main__":
    unittest.main()
