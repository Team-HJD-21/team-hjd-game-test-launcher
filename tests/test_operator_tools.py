"""Operator tools are checked with synthetic private storage; no production launch."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import unittest
import uuid

PROJECT = Path(__file__).resolve().parents[1]
PWSH = shutil.which("pwsh")


@unittest.skipUnless(os.name == "nt" and PWSH, "Windows PowerShell operator tools")
class OperatorToolsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Keep the autostart fixture within Windows Run's 260-character bound.
        cls.root = PROJECT / ".local" / ("operator-test-" + uuid.uuid4().hex[:8])
        cls.root.mkdir(parents=True)

    def setUp(self):
        self.private = self.root / uuid.uuid4().hex[:8]
        self.private.mkdir()

    def run_ps(self, code, *, good=True):
        prefix = "$ErrorActionPreference='Stop';[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding=[Console]::OutputEncoding;"
        environment = dict(os.environ, HJD_TOOL_TEST_ROOT=str(self.private), HJD_TOOL_PROJECT=str(PROJECT))
        result = subprocess.run([PWSH, "-NoLogo", "-NoProfile", "-EncodedCommand", base64.b64encode((prefix + code).encode("utf-16-le")).decode()], env=environment, cwd=PROJECT, text=True, encoding="utf-8", stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if good:
            self.assertEqual(result.returncode, 0, result.stderr[:3000])
        else:
            self.assertNotEqual(result.returncode, 0, "Invalid operation unexpectedly succeeded")
        return result

    def tools(self, expression):
        return "Join-Path $env:HJD_TOOL_PROJECT 'tools/" + expression + "'"

    def catalog_fixture(self):
        self.run_ps("$env:HJD_FIXTURE_PASSWORD='Synthetic-operator-only';& (" + self.tools("New-Publisher.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -ServerUrl https://synthetic.invalid/ -PasswordEnvironmentVariable HJD_FIXTURE_PASSWORD |Out-Null;" +
                    "& (" + self.tools("Write-SignedCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Game @{id='fixture';name='Fixture';version='1.0.0';url='packages/fixture.zip';sha256=('a'*64);bytes=15;files=@(@{path='fixture.exe';bytes=15;sha256=('b'*64)})}|Out-Null")
        (self.private / "packages/fixture.zip").write_bytes(b"synthetic archive")

    def payload(self):
        envelope = json.loads((self.private / "catalog.json").read_text(encoding="utf-8-sig"))
        return json.loads(base64.b64decode(envelope["payload"]))

    def test_all_operator_scripts_parse(self):
        self.run_ps("$errors=@();Get-ChildItem -LiteralPath (Join-Path $env:HJD_TOOL_PROJECT 'tools') -Filter '*.ps1'|ForEach-Object{$itemErrors=$null;[Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$null,[ref]$itemErrors)|Out-Null;$errors+=@($itemErrors)};if($errors.Count){throw 'Operator script parse failure'}")

    def test_catalog_noop_then_renew_preserves_games_and_archives(self):
        self.catalog_fixture()
        catalog = self.private / "catalog.json"
        original = catalog.read_bytes()
        old = self.payload()
        archive_hash = hashlib.sha256((self.private / "packages/fixture.zip").read_bytes()).hexdigest()
        result = self.run_ps("& (" + self.tools("Renew-DistributionCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT|ConvertTo-Json -Compress")
        self.assertFalse(json.loads(result.stdout)["Renewed"])
        self.assertEqual(original, catalog.read_bytes())
        # Set a short expiry and custom metadata, signing with the fixture DPAPI key.
        self.run_ps(". (" + self.tools("CatalogCommon.ps1") + ");$key=Open-CatalogKey $env:HJD_TOOL_TEST_ROOT;try{$path=Join-Path $env:HJD_TOOL_TEST_ROOT 'catalog.json';$p=Read-VerifiedCatalog $path $key;$p.expiresAt=[DateTimeOffset]::UtcNow.AddHours(12).ToString('o');$p.custom=@{retained='yes'};Save-SignedCatalog $path $p $key}finally{$key.Dispose()}; & (" + self.tools("Renew-DistributionCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT|ConvertTo-Json -Compress")
        new = self.payload()
        self.assertEqual(old["sequence"] + 1, new["sequence"])
        self.assertEqual(old["games"], new["games"])
        self.assertEqual(new["custom"], {"retained": "yes"})
        self.assertEqual(archive_hash, hashlib.sha256((self.private / "packages/fixture.zip").read_bytes()).hexdigest())
        self.run_ps(". (" + self.tools("CatalogCommon.ps1") + ");$key=Open-CatalogKey $env:HJD_TOOL_TEST_ROOT;try{$null=Read-VerifiedCatalog (Join-Path $env:HJD_TOOL_TEST_ROOT 'catalog.json') $key}finally{$key.Dispose()}")

    def test_catalog_bad_signature_and_lock_fail_without_changes(self):
        self.catalog_fixture()
        self.run_ps(". (" + self.tools("HostingCommon.ps1") + ");$lock=Open-HostingLock (Join-Path $env:HJD_TOOL_TEST_ROOT 'catalog-publish.lock');try{& (" + self.tools("Renew-DistributionCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Force}finally{$lock.Dispose()}", good=False)
        catalog = self.private / "catalog.json"
        envelope = json.loads(catalog.read_text(encoding="utf-8-sig"))
        payload = json.loads(base64.b64decode(envelope["payload"]))
        payload["sequence"] += 1
        envelope["payload"] = base64.b64encode(json.dumps(payload).encode()).decode()
        catalog.write_text(json.dumps(envelope), encoding="utf-8")
        broken = catalog.read_bytes()
        self.run_ps("& (" + self.tools("Renew-DistributionCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Force", good=False)
        self.assertEqual(broken, catalog.read_bytes())

    def test_publication_uses_locked_verified_catalog(self):
        self.catalog_fixture()
        self.run_ps("& (" + self.tools("Write-SignedCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Game @{id='second';version='2.0';files=@()}|Out-Null")
        self.assertEqual(self.payload()["sequence"], 2)
        self.assertEqual(len(self.payload()["games"]), 2)
        build = self.private / "fixture-build"
        build.mkdir()
        (build / "fixture.exe").write_bytes(b"synthetic build executable, never run")
        self.run_ps("& (" + self.tools("Publish-Game.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -BuildDirectory (Join-Path $env:HJD_TOOL_TEST_ROOT 'fixture-build') -Id third -Name Fixture -Version 3.0.0 -Executable fixture.exe|Out-Null")
        payload = self.payload()
        self.assertEqual(payload["sequence"], 3)
        self.assertEqual(len(payload["games"]), 3)
        published = next(game for game in payload["games"] if game["id"] == "third")
        package = self.private / published["url"]
        self.assertEqual(hashlib.sha256(package.read_bytes()).hexdigest(), published["sha256"])

    def test_process_identity_rejects_reused_pid_and_wrong_executable(self):
        self.run_ps(". (" + self.tools("HostingCommon.ps1") + ");$current=Get-Process -Id $PID;$state=@{pid=$PID;startedTicks='0'};if(Get-OwnedHostingProcess $state $current.Path @()){throw 'Stale PID was accepted'}")
        self.run_ps(". (" + self.tools("HostingCommon.ps1") + ");$current=Get-Process -Id $PID;$state=@{pid=$PID;startedTicks=$current.StartTime.ToUniversalTime().Ticks.ToString()};Get-OwnedHostingProcess $state (Join-Path $env:HJD_TOOL_TEST_ROOT 'different.exe') @()", good=False)

    def test_mac_variant_preserves_windows_and_survives_windows_update(self):
        self.catalog_fixture()
        windows = self.payload()["games"][0]
        self.run_ps("& (" + self.tools("Write-SignedCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Game @{id='fixture';name='Fixture';version='1.0.0';platform='macos-universal';executable='Fixture.app/Contents/MacOS/Fixture';url='packages/mac.zip';sha256=('c'*64);bytes=21;files=@()}|Out-Null")
        combined = self.payload()["games"][0]
        self.assertEqual({k: v for k, v in combined.items() if k != 'platforms'}, windows)
        mac = combined['platforms']['macos-universal']
        self.assertEqual(mac['executable'], 'Fixture.app/Contents/MacOS/Fixture')
        self.run_ps("& (" + self.tools("Write-SignedCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Game @{id='fixture';name='Fixture';version='1.1.0';url='packages/new-windows.zip';files=@()}|Out-Null")
        updated = self.payload()["games"][0]
        self.assertEqual(updated['version'], '1.1.0')
        self.assertEqual(updated['platforms']['macos-universal'], mac)
        before = (self.private / 'catalog.json').read_bytes()
        self.run_ps("& (" + self.tools("Write-SignedCatalog.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -Game @{id='missing';name='Fixture';platform='macos-universal'}", good=False)
        self.assertEqual(before, (self.private / 'catalog.json').read_bytes())

    def test_verified_report_empty_and_explicit_csv(self):
        result = self.run_ps("& (" + self.tools("Get-VerifiedTesters.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -AsJson")
        self.assertEqual(json.loads(result.stdout), [])
        audit = {"schema": 1, "users": [{"email": "=fixture@example.invalid", "firstVerifiedAt": "2026-10-29T00:00:00Z", "lastVerifiedAt": "2026-10-29T00:00:00Z", "lastAccessAt": "2026-10-29T01:00:00Z", "loginCount": 1, "source": "cloudflare-access-otp", "lastSessionHash": "secret-session-hash", "activeSessions": []}]}
        (self.private / "verified-users.json").write_text(json.dumps(audit), encoding="utf-8")
        result = self.run_ps("& (" + self.tools("Get-VerifiedTesters.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -AsJson")
        self.assertEqual(len(json.loads(result.stdout)), 1)
        self.assertNotIn("secret-session-hash", result.stdout)
        self.assertFalse((self.private / "verified.csv").exists())
        self.run_ps("& (" + self.tools("Get-VerifiedTesters.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -CsvPath (Join-Path $env:HJD_TOOL_TEST_ROOT 'verified.csv')|Out-Null")
        csv = (self.private / "verified.csv").read_text(encoding="utf-8-sig")
        self.assertIn("'=fixture@example.invalid", csv)
        self.assertNotIn("secret-session-hash", csv)
        self.run_ps("& (" + self.tools("Get-VerifiedTesters.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -CsvPath (Join-Path $env:HJD_TOOL_TEST_ROOT 'verified.csv')|Out-Null", good=False)

    def test_python_selection_preflight_has_no_runtime_writes(self):
        for relative in ("auth.json", "catalog.json", "tls/server-chain.pem", "tls/server-key.pem"):
            path = self.private / relative
            path.parent.mkdir(exist_ok=True)
            path.write_text("synthetic fixture", encoding="utf-8")
        invoke = "& (" + self.tools("Start-Distribution.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -ValidateOnly|ConvertTo-Json -Compress"
        legacy = json.loads(self.run_ps(invoke).stdout)
        self.assertFalse(legacy["CloudflareEnabled"])
        (self.private / "cloudflare-access.json").write_text(json.dumps({"enabled": True, "issuer": "https://synthetic-team.cloudflareaccess.com", "audience": "synthetic-application-audience"}), encoding="utf-8")
        enabled = json.loads(self.run_ps(invoke).stdout)
        self.assertTrue(enabled["Python"].replace("\\", "/").endswith(".deps/server-venv/Scripts/python.exe"))
        self.assertFalse((self.private / "server-process.json").exists())
        self.assertFalse((self.private / "runtime-logs").exists())
        cf_root = self.private / "cloudflare"
        cf_root.mkdir()
        (cf_root / "tunnel-token.txt").write_text("synthetic-nonsecret-token-fixture", encoding="utf-8")
        hosted = json.loads(self.run_ps("& (" + self.tools("Start-PlaytestHosting.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -ValidateOnly|ConvertTo-Json -Compress").stdout)
        self.assertEqual(hosted["Bind"], "127.0.0.1")
        self.assertTrue(hosted["ManagedTokenFile"])
        self.assertFalse((cf_root / "managed-connector.yml").exists())
        self.assertFalse((self.private / "hosting-monitor.json").exists())
        self.assertNotIn("synthetic-nonsecret-token-fixture", json.dumps(hosted))
        autostart = json.loads(self.run_ps("& (" + self.tools("Enable-HostingAutostart.ps1") + ") -PrivateRoot $env:HJD_TOOL_TEST_ROOT -ValidateOnly|ConvertTo-Json -Depth 5 -Compress").stdout)
        self.assertTrue(autostart["ValidationOnly"])
        self.assertFalse(autostart["RequiresAdministrator"])
        self.assertLess(len(autostart["Command"]), 260)
        self.assertTrue(autostart["RegistryKey"].startswith("HKCU\\"))
        self.assertFalse((self.private / "hosting-autostart.ps1").exists())


if __name__ == "__main__":
    unittest.main()
