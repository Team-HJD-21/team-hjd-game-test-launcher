"""Loopback capability/CSRF boundaries and owned-process stop regression tests."""
import base64
import hashlib
import http.client
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import sys
import threading
import time
import unittest
from urllib.parse import urlencode
import uuid

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT))
from server.hosting_control import create_server, HostingController


class FakeController:
    def __init__(self):
        self.actions = []

    def refresh(self):
        pass

    def operate(self, action):
        self.actions.append(action)
        if action == "restart":
            raise RuntimeError("PRIVATE_ERROR_SENTINEL")


class LocalControlTests(unittest.TestCase):
    def setUp(self):
        self.root = PROJECT / ".local" / ("control-test-" + uuid.uuid4().hex)
        (self.root / "dashboard").mkdir(parents=True)
        (self.root / "dashboard/index.html").write_text('<html><head><meta http-equiv="refresh" content="15"></head><body><!--CONTROL-FEEDBACK-->tester@example.invalid</body></html>', encoding="utf-8")
        (self.root / "dashboard/verified-testers.csv").write_text("Email\r\ntester@example.invalid", encoding="utf-8")
        self.token = secrets.token_hex(32)
        self.controller = FakeController()
        self.server = create_server(self.root, 0, self.token, self.controller)
        self.port = self.server.server_port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(3)

    def request(self, method, path="/", body=None, **headers):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        status, result_headers, data = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return status, result_headers, data

    def post(self, token=None, action="refresh", origin="null", **headers):
        headers = {"Content-Type": "application/x-www-form-urlencoded", "Origin": origin, **headers}
        return self.request("POST", "/control", urlencode({"token": token if token is not None else self.token, "action": action}), **headers)

    def test_private_reads_and_no_mutating_get(self):
        self.assertEqual(self.request("GET", "/health")[0], 204)
        for route in ("/", "/verified-testers.csv", "/control?action=stop"):
            status, headers, body = self.request("GET", route)
            self.assertEqual(status, 401)
            self.assertNotIn(b"tester@example.invalid", body)
            self.assertNotIn("Access-Control-Allow-Origin", headers)
        self.assertEqual(self.controller.actions, [])

    def test_host_origin_token_and_form_boundaries(self):
        for options in ({"token": "0" * 64}, {"token": "한글"}, {"origin": "https://attacker.invalid"}, {"Host": f"localhost:{self.port}"}, {"action": "arbitrary-command"}):
            self.assertEqual(self.post(**options)[0], 403)
        self.assertEqual(self.request("POST", "/control", "token=x&action=stop", **{"Content-Type": "application/x-www-form-urlencoded"})[0], 403)
        self.assertEqual(self.request("POST", "/control", "x" * 5000, **{"Content-Type": "application/x-www-form-urlencoded", "Origin": "null"})[0], 403)
        self.assertEqual(self.controller.actions, [])

    def test_file_bootstrap_cookie_same_origin_actions_and_csv(self):
        status, headers, body = self.post()
        self.assertEqual(status, 200)
        self.assertIn(b"tester@example.invalid", body)
        self.assertIn(b'content="15;url=/"', body)
        self.assertIn("HttpOnly; SameSite=Strict", headers["Set-Cookie"])
        cookie = headers["Set-Cookie"].split(";", 1)[0]
        self.assertEqual(self.request("GET", "/", Cookie=cookie)[0], 200)
        self.assertEqual(self.request("GET", "/verified-testers.csv", Cookie=cookie)[0], 200)
        self.assertEqual(self.request("GET", "/../auth.json", Cookie=cookie)[0], 404)
        self.assertEqual(self.post(token="bad", Cookie=cookie)[0], 403)
        self.assertEqual(self.post(action="stop", origin=f"http://127.0.0.1:{self.port}", Cookie=cookie)[0], 200)
        self.assertEqual(self.controller.actions, ["refresh", "stop"])

    def test_errors_are_fixed_messages_and_service_remains_usable(self):
        status, _, body = self.post(action="restart")
        self.assertEqual(status, 503)
        self.assertNotIn(b"PRIVATE_ERROR_SENTINEL", body)
        self.assertEqual(self.post()[0], 200)

    def test_saved_launch_parameters_are_preserved_and_checked_before_restart(self):
        controller = HostingController(self.root, Path('pwsh.exe'), 8443)
        calls = []
        controller.run = lambda name, arguments=(): calls.append((name, tuple(arguments)))
        path = self.root / 'hosting-launch.json'
        launch = {'PrivateRoot': str(self.root), 'Port': 8443, 'TokenFile': str(self.root / 'custom-token-file.txt'), 'PythonPath': str(self.root / 'custom-python.exe')}
        path.write_text(json.dumps(launch), encoding='utf-8')
        controller.operate('restart')
        self.assertEqual(calls[0][0], 'Stop-PlaytestHosting.ps1')
        self.assertIn(str(self.root / 'custom-token-file.txt'), calls[1][1])
        calls.clear()
        launch['Port'] = 8444
        path.write_text(json.dumps(launch), encoding='utf-8')
        with self.assertRaises(ValueError):
            controller.operate('restart')
        self.assertEqual(calls, [])

    def test_browser_handoff_is_short_lived_single_use_and_redirects_without_code(self):
        code = secrets.token_hex(32)
        handoff = {'sha256': hashlib.sha256(code.encode()).hexdigest(), 'expiresAt': int(time.time()) + 60, 'used': False}
        path = self.root / 'dashboard-open.json'
        path.write_text(json.dumps(handoff), encoding='utf-8')
        route = '/open/' + code
        status, headers, _ = self.request('GET', route)
        self.assertEqual(status, 303)
        self.assertEqual(headers['Location'], '/')
        self.assertNotIn(code, headers['Set-Cookie'])
        self.assertEqual(self.request('GET', route)[0], 401)
        handoff['expiresAt'] = int(time.time()) - 1
        path.write_text(json.dumps(handoff), encoding='utf-8')
        self.assertEqual(self.request('GET', route)[0], 401)


PWSH = shutil.which("pwsh")


@unittest.skipUnless(os.name == "nt" and PWSH, "Windows process identity")
class OwnedStopTests(unittest.TestCase):
    def setUp(self):
        self.root = PROJECT / ".local" / ("owned-stop-test-" + uuid.uuid4().hex)
        self.root.mkdir(parents=True)
        self.process = None

    def tearDown(self):
        if self.process is not None:
            if self.process.poll() is None:
                self.process.terminate()
            self.process.communicate(timeout=5)

    def ps(self, script, **environment):
        prefix = "$ErrorActionPreference='Stop';[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding=[Console]::OutputEncoding;"
        command = base64.b64encode((prefix + script).encode("utf-16-le")).decode()
        return subprocess.run([PWSH, "-NoLogo", "-NoProfile", "-EncodedCommand", command], cwd=PROJECT,
                              env=dict(os.environ, HJD_STOP_ROOT=str(self.root), HJD_STOP_PROJECT=str(PROJECT), **environment),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8", timeout=30,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))

    def start_fixture(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            self.port = listener.getsockname()[1]
        salt = secrets.token_bytes(32)
        (self.root / "auth.json").write_text(json.dumps({"salt": base64.b64encode(salt).decode(), "hash": base64.b64encode(hashlib.pbkdf2_hmac("sha256", b"synthetic-only", salt, 100000)).decode(), "iterations": 100000}), encoding="utf-8")
        (self.root / "catalog.json").write_text('{"fixture":true}', encoding="utf-8")
        self.process = subprocess.Popen([sys._base_executable, str(PROJECT / "server/distribution_server.py"), "--root", str(self.root),
                                       "--bind", "127.0.0.1", "--port", str(self.port), "--test-http"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=.1):
                    break
            except OSError:
                time.sleep(.05)
        else:
            self.fail("Synthetic origin did not listen")
        result = self.ps(". ./tools/HostingCommon.ps1; $p=Get-Process -Id ([int]$env:HJD_STOP_PID); Write-HostingJson (Join-Path $env:HJD_STOP_ROOT 'server-process.json') @{pid=$p.Id;startedTicks=$p.StartTime.ToUniversalTime().Ticks.ToString();executable=$p.Path;script=(Join-Path $env:HJD_STOP_PROJECT 'server/distribution_server.py');bind='127.0.0.1';port=[int]$env:HJD_STOP_PORT}", HJD_STOP_PID=str(self.process.pid), HJD_STOP_PORT=str(self.port))
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_validate_only_and_stop_preserve_other_state_and_user_files(self):
        self.start_fixture()
        (self.root / "keep.txt").write_text("PRESERVE", encoding="utf-8")
        result = self.ps("& ./tools/Stop-PlaytestHosting.ps1 -PrivateRoot $env:HJD_STOP_ROOT -ValidateOnly | ConvertTo-Json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(self.process.poll())
        self.assertFalse((self.root / "hosting-start.lock").exists())
        result = self.ps("& ./tools/Stop-PlaytestHosting.ps1 -PrivateRoot $env:HJD_STOP_ROOT | ConvertTo-Json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.process.wait(timeout=5)
        self.assertIn('"AutostartPreserved": true', result.stdout)
        self.assertEqual((self.root / "keep.txt").read_text(encoding="utf-8"), "PRESERVE")

    def test_stale_pid_and_wrong_arguments_do_not_kill(self):
        self.start_fixture()
        state_path = self.root / "server-process.json"
        state = json.loads(state_path.read_text(encoding="utf-8-sig"))
        original_ticks = state["startedTicks"]
        state["startedTicks"] = "0"
        state_path.write_text(json.dumps(state), encoding="utf-8")
        result = self.ps("& ./tools/Stop-PlaytestHosting.ps1 -PrivateRoot $env:HJD_STOP_ROOT | Out-Null")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(self.process.poll())
        state["startedTicks"] = original_ticks
        state["port"] = self.port + 1 if self.port < 65535 else self.port - 1
        state_path.write_text(json.dumps(state), encoding="utf-8")
        result = self.ps("& ./tools/Stop-PlaytestHosting.ps1 -PrivateRoot $env:HJD_STOP_ROOT | Out-Null")
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(self.process.poll())


if __name__ == "__main__":
    unittest.main()
