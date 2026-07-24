#!/usr/bin/env python3

import http.client
import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.parse import urlencode

import web_mock_server


class WebContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(
            ("127.0.0.1", 0), web_mock_server.DeskMockHandler
        )
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.host = f"127.0.0.1:{cls.server.server_port}"
        cls.origin = f"https://{cls.host}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=2)

    def request(self, method, path, body=None, headers=None):
        connection = http.client.HTTPConnection(
            "127.0.0.1", self.server.server_port, timeout=2
        )
        request_headers = {"Host": self.host}
        if headers is not None:
            request_headers.update(headers)
        connection.request(method, path, body=body, headers=request_headers)
        response = connection.getresponse()
        payload = response.read()
        result = response.status, response.getheaders(), payload
        connection.close()
        return result

    def login(self):
        body = urlencode({"token": "0123456789abcdef0123456789abcdef"})
        status, headers, payload = self.request(
            "POST",
            "/api/v1/session",
            body,
            {"Content-Type": "application/x-www-form-urlencoded"},
        )
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(payload), {"authenticated": True})
        cookie = dict(headers)["Set-Cookie"]
        self.assertTrue(cookie.startswith(web_mock_server.SESSION_COOKIE))
        self.assertIn("Secure", cookie)
        self.assertIn("HttpOnly", cookie)
        self.assertIn("SameSite=Strict", cookie)
        return web_mock_server.SESSION_COOKIE

    def authenticated_headers(self, content_type=None):
        headers = {
            "Cookie": self.login(),
            "Origin": self.origin,
        }
        if content_type is not None:
            headers["Content-Type"] = content_type
        return headers

    def test_assets_and_unauthenticated_status(self):
        for path, content_type in (
            ("/", "text/html"),
            ("/app.css", "text/css"),
            ("/app.js", "application/javascript"),
        ):
            status, headers, payload = self.request("GET", path)
            self.assertEqual(status, 200)
            self.assertTrue(dict(headers)["Content-Type"].startswith(content_type))
            self.assertGreater(len(payload), 100)

        status, _, payload = self.request("GET", "/api/v1/status")
        self.assertEqual(status, 401)
        self.assertEqual(json.loads(payload)["error"], "authentication_required")

    def test_authenticated_status_and_diagnostics_contract(self):
        cookie = self.login()
        status, _, payload = self.request(
            "GET", "/api/v1/status", headers={"Cookie": cookie}
        )
        self.assertEqual(status, 200)
        snapshot = json.loads(payload)
        self.assertEqual(len(snapshot["axes"]), 3)
        self.assertEqual(len(snapshot["presets"]), 3)
        self.assertTrue(snapshot["referenced"])
        self.assertFalse(snapshot["motion_inhibited"])
        self.assertIn(snapshot["state"], {"idle", "unreferenced", "error"})

        status, _, payload = self.request(
            "GET", "/api/v1/diagnostics", headers={"Cookie": cookie}
        )
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(payload), {"events": []})

    def test_mutating_endpoints_require_same_origin(self):
        cookie = self.login()
        form_headers = {
            "Cookie": cookie,
            "Content-Type": "application/x-www-form-urlencoded",
        }
        status, _, _ = self.request(
            "POST", "/api/v1/mode", urlencode({"mode": "mobile"}), form_headers
        )
        self.assertEqual(status, 401)

        form_headers["Origin"] = self.origin
        for path, fields in (
            ("/api/v1/mode", {"mode": "hybrid"}),
            ("/api/v1/preset", {"slot": "2"}),
            ("/api/v1/provision", {"ssid": "desk-lab", "password": "safe-pass"}),
        ):
            status, _, payload = self.request(
                "POST", path, urlencode(fields), form_headers
            )
            self.assertEqual(status, 200)
            self.assertTrue(json.loads(payload))

        status, _, payload = self.request(
            "DELETE",
            "/api/v1/session",
            headers={"Cookie": cookie, "Origin": self.origin},
        )
        self.assertEqual(status, 200)
        self.assertFalse(json.loads(payload)["authenticated"])

    def test_signed_update_contract_rejects_wrong_transport_shape(self):
        cookie = self.login()
        status, _, _ = self.request(
            "POST",
            "/api/v1/update",
            b"firmware",
            {
                "Cookie": cookie,
                "Origin": "https://attacker.invalid",
                "Content-Type": "application/octet-stream",
            },
        )
        self.assertEqual(status, 403)

        status, _, _ = self.request(
            "POST",
            "/api/v1/update",
            b"firmware",
            {
                "Cookie": cookie,
                "Origin": self.origin,
                "Content-Type": "text/plain",
            },
        )
        self.assertEqual(status, 403)

        status, _, payload = self.request(
            "POST",
            "/api/v1/update",
            b"signed-test-image",
            {
                "Cookie": cookie,
                "Origin": self.origin,
                "Content-Type": "application/octet-stream",
            },
        )
        self.assertEqual(status, 200)
        self.assertTrue(json.loads(payload)["rebooting"])


if __name__ == "__main__":
    unittest.main()
