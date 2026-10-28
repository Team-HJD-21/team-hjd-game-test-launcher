"""Local private HTML uses synthetic storage and never starts a production service."""
import base64
import hashlib
import html
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time
import unittest
import uuid

PROJECT = Path(__file__).resolve().parents[1]
PWSH = shutil.which("pwsh")


@unittest.skipUnless(os.name == "nt" and PWSH, "Windows hosting dashboard tools")
class DashboardTests(unittest.TestCase):
    def setUp(self):
        self.root = PROJECT / ".local" / ("dashboard-test-" + uuid.uuid4().hex[:12])
        self.root.mkdir(parents=True)
        self.server = None

    def tearDown(self):
        if self.server is not None:
            self.server.terminate()
            self.server.communicate(timeout=5)

    def ps(self, code, **values):
        prefix = "$ErrorActionPreference='Stop';[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding=[Console]::OutputEncoding;"
        environment = dict(os.environ, HJD_DASH_ROOT=str(self.root), HJD_DASH_PROJECT=str(PROJECT), **values)
        result = subprocess.run([PWSH, "-NoLogo", "-NoProfile", "-EncodedCommand", base64.b64encode((prefix + code).encode("utf-16-le")).decode()], env=environment, cwd=PROJECT, encoding="utf-8", stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=35, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.assertEqual(result.returncode, 0, result.stderr[:3000])
        return result.stdout

    def fixture(self):
        self.ps("$env:HJD_DASH_PASSWORD='Synthetic-dashboard-password'; & (Join-Path $env:HJD_DASH_PROJECT 'tools/New-Publisher.ps1') -PrivateRoot $env:HJD_DASH_ROOT -ServerUrl https://synthetic.invalid/ -PasswordEnvironmentVariable HJD_DASH_PASSWORD|Out-Null; & (Join-Path $env:HJD_DASH_PROJECT 'tools/Write-SignedCatalog.ps1') -PrivateRoot $env:HJD_DASH_ROOT -Game @{id='fixture';name='<img src=x onerror=alert(1)>';version='1.2.3';bytes=12345;files=@()}|Out-Null")

    def generate(self, extra=""):
        self.ps("& (Join-Path $env:HJD_DASH_PROJECT 'tools/Open-HostingDashboard.ps1') -PrivateRoot $env:HJD_DASH_ROOT -GenerateOnly -SkipPublicCheck " + extra + "|Out-Null")
        return (self.root / "dashboard/index.html").read_text(encoding="utf-8")

    def test_empty_users_escaping_csp_and_explicit_safe_csv(self):
        self.fixture()
        sentinel = "eyJsynthetic-unshared-token.NEVER-OUTPUT.SIGNATURE"
        (self.root / "irrelevant-private.json").write_text(json.dumps({"jwt": sentinel, "privateKey": "PRIVATE_KEY_SENTINEL", "ip": "198.51.100.73"}), encoding="utf-8")
        page = self.generate()
        self.assertIn("아직 인증 후 접속한 테스터가 없습니다", page)
        self.assertIn("서명 검증됨", page)
        self.assertIn(html.escape("<img src=x onerror=alert(1)>", quote=True), page)
        self.assertNotIn("<img src=x", page)
        self.assertFalse((self.root / "dashboard/verified-testers.csv").exists())
        invite = json.loads((self.root / "invite.json").read_text(encoding="utf-8-sig"))
        self.assertNotIn(invite["publisherKey"], page)
        audit = {"schema": 1, "users": [{"email": '=tester+<script>alert("x")</script>@example.invalid', "firstVerifiedAt": "2026-10-29T00:00:00Z", "lastVerifiedAt": "2026-10-29T01:00:00Z", "lastAccessAt": "2026-10-29T02:00:00Z", "loginCount": 2, "source": "cloudflare-access-otp", "lastSessionHash": "DO_NOT_RENDER_SESSION_HASH", "rawJWT": sentinel}]}
        (self.root / "verified-users.json").write_text(json.dumps(audit), encoding="utf-8")
        page = self.generate("-ExportCsv")
        for secret in (sentinel, "PRIVATE_KEY_SENTINEL", "DO_NOT_RENDER_SESSION_HASH", "198.51.100.73", invite["publisherKey"]):
            self.assertNotIn(secret, page)
        self.assertIn("2026.10.29 09:00:00", page)
        self.assertIn("UTC 2026-10-29 00:00:00", page)
        self.assertIn("&lt;script&gt;", page)
        self.assertIn('href="verified-testers.csv"', page)
        csv = (self.root / "dashboard/verified-testers.csv").read_text(encoding="utf-8-sig")
        self.assertIn("'=tester+", csv)
        self.assertNotIn(sentinel, csv)
        class Structure(HTMLParser):
            def __init__(self):
                super().__init__()
                self.scripts = []
                self.csp = ""
                self.style = ""
                self.in_style = False
            def handle_starttag(self, tag, attrs):
                attributes = dict(attrs)
                if tag == "script":
                    self.scripts.append(attributes)
                if tag == "meta" and attributes.get("http-equiv") == "Content-Security-Policy":
                    self.csp = attributes["content"]
                if tag == "style":
                    self.in_style = True
            def handle_endtag(self, tag):
                if tag == "style":
                    self.in_style = False
            def handle_data(self, data):
                if self.in_style:
                    self.style += data
        structure = Structure()
        structure.feed(page)
        self.assertEqual(structure.scripts, [])
        self.assertIn("default-src 'none'", structure.csp)
        self.assertIn("script-src 'none'", structure.csp)
        self.assertIn("sha256-" + base64.b64encode(hashlib.sha256(structure.style.encode()).digest()).decode(), structure.csp)

    def test_live_synthetic_origin_and_stale_pid_signature_rejection(self):
        self.fixture()
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        self.server = subprocess.Popen([sys._base_executable, str(PROJECT / "server/distribution_server.py"), "--root", str(self.root), "--bind", "127.0.0.1", "--port", str(port), "--test-http"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=.2):
                    break
            except OSError:
                time.sleep(.05)
        else:
            self.fail("Synthetic dashboard origin did not start")
        self.ps(". (Join-Path $env:HJD_DASH_PROJECT 'tools/HostingCommon.ps1');$p=Get-Process -Id ([int]$env:HJD_DASH_PID);Write-HostingJson (Join-Path $env:HJD_DASH_ROOT 'server-process.json') @{pid=$p.Id;startedTicks=$p.StartTime.ToUniversalTime().Ticks.ToString();executable=$p.Path;python=$p.Path;script=(Join-Path $env:HJD_DASH_PROJECT 'server/distribution_server.py');bind='127.0.0.1';port=[int]$env:HJD_DASH_PORT}", HJD_DASH_PID=str(self.server.pid), HJD_DASH_PORT=str(port))
        page = self.generate()
        self.assertIn('data-component="origin" data-state="running"', page)
        self.assertIn("루프백 TCP 연결 확인", page)
        state_path = self.root / "server-process.json"
        state = json.loads(state_path.read_text(encoding="utf-8-sig"))
        state["startedTicks"] = "0"
        state_path.write_text(json.dumps(state), encoding="utf-8")
        envelope_path = self.root / "catalog.json"
        envelope = json.loads(envelope_path.read_text(encoding="utf-8-sig"))
        envelope["signature"] = base64.b64encode(bytes(64)).decode()
        envelope_path.write_text(json.dumps(envelope), encoding="utf-8")
        page = self.generate()
        self.assertIn('data-component="origin" data-state="stopped"', page)
        self.assertNotIn("루프백 TCP 연결 확인", page)
        self.assertIn("서명 / 목록 확인 불가", page)
        self.assertNotIn("&lt;img src=x", page)


if __name__ == "__main__":
    unittest.main()
