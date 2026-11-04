from datetime import datetime, timezone
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'server'))
from cloudflare_auth import AuditUnavailable, CloudflareAccess, VerifiedIdentity, _atomic_json, _file_lock

class RetentionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / 'verified-users.json'
        self.auth = CloudflareAccess(self.root)
        self.now = datetime(2026, 11, 5, tzinfo=timezone.utc).timestamp()
    def record(self, email, days, *, seconds=0):
        stamp = datetime.fromtimestamp(self.now - days * 86400 + seconds, timezone.utc).isoformat(timespec='seconds').replace('+00:00', 'Z')
        return dict(email=email, firstVerifiedAt=stamp, lastVerifiedAt=stamp, lastAccessAt=stamp, loginCount=1,
                    lastSessionHash='a' * 64, activeSessions=[], source='cloudflare-access-otp')
    def save(self, users):
        _atomic_json(self.path, dict(schema=1, users=users))
    def test_preview_and_exact_boundary(self):
        self.save([self.record('old@example.invalid', 180), self.record('keep@example.invalid', 180, seconds=1)])
        before = self.path.read_bytes()
        preview = self.auth.maintain_users(now=self.now)
        self.assertEqual(preview['expired'], 1)
        self.assertFalse(preview['applied'])
        self.assertEqual(self.path.read_bytes(), before)
        result = self.auth.maintain_users(apply=True, now=self.now)
        self.assertTrue(result['applied'])
        self.assertEqual(result['remaining'], 1)
        self.assertNotIn('email', json.dumps(result))
        self.assertEqual(json.loads(self.path.read_text(encoding='utf-8-sig'))['users'][0]['email'], 'keep@example.invalid')
    def test_remove_one_case_insensitive_identity_only(self):
        self.save([self.record('one@example.invalid', 1), self.record('two@example.invalid', 2)])
        result = self.auth.maintain_users(apply=True, remove_email='ONE@example.invalid', now=self.now)
        self.assertEqual(result['requestedRemoval'], 1)
        self.assertEqual(result['remaining'], 1)
        self.assertEqual(json.loads(self.path.read_text(encoding='utf-8-sig'))['users'][0]['email'], 'two@example.invalid')
    def test_bad_and_future_times_preserve_original(self):
        for value in ('not-a-date', '2026-11-06T00:00:00Z', '2026-11-05T09:00:00+09:00'):
            record = self.record('one@example.invalid', 1)
            record['lastAccessAt'] = value
            self.save([record]);before = self.path.read_bytes()
            with self.assertRaises(AuditUnavailable):
                self.auth.maintain_users(apply=True, now=self.now)
            self.assertEqual(self.path.read_bytes(), before)
    def test_corrupt_schema_sessions_and_duplicates_preserve_original(self):
        good = self.record('one@example.invalid', 1)
        cases = [dict(schema=1, users='broken'), dict(schema=1, users=[good, copy.deepcopy(good)])]
        bad = copy.deepcopy(good);bad['activeSessions'] = [dict(hash='bad', expiresAt=123)]
        cases.append(dict(schema=1, users=[bad]))
        for data in cases:
            _atomic_json(self.path, data);before = self.path.read_bytes()
            with self.assertRaises(AuditUnavailable):
                self.auth.maintain_users(apply=True, now=self.now)
            self.assertEqual(self.path.read_bytes(), before)
    def test_write_failure_preserves_original(self):
        self.save([self.record('old@example.invalid', 181)]);before = self.path.read_bytes()
        with mock.patch('cloudflare_auth._atomic_json', side_effect=OSError('synthetic write failure')):
            with self.assertRaises(AuditUnavailable):
                self.auth.maintain_users(apply=True, now=self.now)
        self.assertEqual(self.path.read_bytes(), before)
    def test_other_operator_lock_preserves_original(self):
        self.save([self.record('old@example.invalid', 181)]);before = self.path.read_bytes()
        with _file_lock(self.root / 'verified-users.lock'):
            with self.assertRaises(AuditUnavailable):
                self.auth.maintain_users(apply=True, now=self.now)
        self.assertEqual(self.path.read_bytes(), before)
    def test_new_authenticated_access_after_deletion_creates_new_record(self):
        self.save([self.record('one@example.invalid', 1)])
        self.auth.maintain_users(apply=True, remove_email='one@example.invalid', now=self.now)
        with mock.patch('cloudflare_auth.time.time', return_value=self.now):
            self.auth.record_access(VerifiedIdentity('one@example.invalid', 'b' * 64, int(self.now) + 3600))
        users = json.loads(self.path.read_text(encoding='utf-8-sig'))['users']
        self.assertEqual(len(users), 1)
        self.assertEqual(users[0]['loginCount'], 1)
    def test_request_path_also_prunes_expired_users(self):
        self.save([self.record('old@example.invalid', 181)])
        with mock.patch('cloudflare_auth.time.time', return_value=self.now):
            self.auth.record_access(VerifiedIdentity('new@example.invalid', 'b' * 64, int(self.now) + 3600))
        users = json.loads(self.path.read_text(encoding='utf-8-sig'))['users']
        self.assertEqual([user['email'] for user in users], ['new@example.invalid'])

if __name__ == '__main__':
    unittest.main()
