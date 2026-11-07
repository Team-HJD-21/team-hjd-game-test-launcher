"""Exercise public Mac artifact audit boundaries without a macOS host."""
from pathlib import Path
import runpy
import hashlib
import json
import os
import stat
import tempfile
import unittest
import zipfile


audit = runpy.run_path(str(Path(__file__).resolve().parents[1] / "tools" / "verify-macos.py"))


class MacPackageAuditTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'posix', 'Real POSIX file modes are required for seed auditing')
    def test_initial_manifest_survives_native_copy_without_relaxing_seed_checks(self):
        with tempfile.TemporaryDirectory() as temp:
            seed = Path(temp, 'Resources/seed')
            seed.mkdir(parents=True)
            entry = seed / 'client'
            entry.write_bytes(b'native')
            entry.chmod(0o644)
            manifest = {'version': '0.2.4', 'executable': 'TeamHJDClient.app/Contents/MacOS/TeamHJDClient',
                        'files': [{'path': 'client', 'bytes': 6, 'sha256': hashlib.sha256(b'native').hexdigest(), 'executableFile': False}]}
            (seed.parent / 'seed-manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
            self.assertEqual(set(audit['receipt_files'](seed)), {'client'})
            entry.write_bytes(b'broken')
            with self.assertRaises(ValueError):
                audit['receipt_files'](seed)
            entry.write_bytes(b'native')
            (seed / '._teamhjd-receipt.json').write_text('{}', encoding='utf-8')
            with self.assertRaises(ValueError):
                audit['receipt_files'](seed)

    def make_zip(self, path, entries):
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, content, mode in entries:
                info = zipfile.ZipInfo(name)
                info.create_system = 3
                info.external_attr = mode << 16
                archive.writestr(info, content)

    def test_unix_execute_permissions_and_payload_bytes_are_audited(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp, "fixture.zip")
            self.make_zip(archive, [("Fixture.app/Contents/MacOS/Fixture", b"native", stat.S_IFREG | 0o755),
                                    ("Fixture.app/Contents/Info.plist", b"data", stat.S_IFREG | 0o644)])
            files = audit["zip_files"](archive)
            self.assertTrue(files["Fixture.app/Contents/MacOS/Fixture"]["executableFile"])
            self.assertFalse(files["Fixture.app/Contents/Info.plist"]["executableFile"])
            self.assertEqual(files["Fixture.app/Contents/MacOS/Fixture"]["bytes"], 6)
            self.assertEqual(len(files["Fixture.app/Contents/MacOS/Fixture"]["sha256"]), 64)

    def test_links_collisions_and_unsafe_modes_fail_before_release(self):
        bad = [
            [("outside", b"../../outside", stat.S_IFLNK | 0o777)],
            [("data", b"x", stat.S_IFREG | 0o644), ("data/file", b"y", stat.S_IFREG | 0o644)],
            [("Data", b"x", stat.S_IFREG | 0o644), ("data", b"y", stat.S_IFREG | 0o644)],
            [("Fixture", b"x", stat.S_IFREG | 0o4755)],
            [("Fixture", b"x", stat.S_IFREG | 0o777)],
        ]
        with tempfile.TemporaryDirectory() as temp:
            for index, entries in enumerate(bad):
                with self.subTest(index=index):
                    archive = Path(temp, f"bad-{index}.zip")
                    self.make_zip(archive, entries)
                    with self.assertRaises(ValueError):
                        audit["zip_files"](archive)

    def test_credentials_and_ambiguous_paths_cannot_enter_public_artifact(self):
        for path in ["../escape", "/escape", "a//b", "a/./b", "a\\b", "a:b", ".local/proof.json",
                     "Contents/Resources/auth.json", "Contents/Resources/tunnel-token.txt", "Contents/key.dpapi"]:
            with self.subTest(path=path), self.assertRaises(ValueError):
                audit["public_path"](path)
        audit["public_path"]("TeamHJDClient.app/Contents/Resources/게임 로고.png")


if __name__ == "__main__":
    unittest.main()
