import json
from email.message import Message
from pathlib import Path
import tempfile
import unittest
from server.access_policy import check_client, PRIVACY_VERSION

class PolicyTests(unittest.TestCase):
    def test_policy_gates_before_user_recording(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); h=Message()
            self.assertEqual(check_client(root,h),(None,""))
            path=root/'client-policy.json'
            path.write_text(json.dumps({'schema':1,'minimumVersion':'0.2.0','privacyVersion':PRIVACY_VERSION}),encoding='utf-8')
            self.assertEqual(check_client(root,h),(426,""))
            h['X-TeamHJD-Launcher-Version']='0.1.4'
            self.assertEqual(check_client(root,h),(426,""))
            h.replace_header('X-TeamHJD-Launcher-Version','0.2.0')
            self.assertEqual(check_client(root,h),(428,""))
            h['X-TeamHJD-Privacy-Version']=PRIVACY_VERSION
            self.assertEqual(check_client(root,h),(None,PRIVACY_VERSION))
            h['X-TeamHJD-Privacy-Version']=PRIVACY_VERSION
            self.assertEqual(check_client(root,h),(428,""))
            del h['X-TeamHJD-Privacy-Version'];h['X-TeamHJD-Privacy-Version']=PRIVACY_VERSION
            h['X-TeamHJD-Launcher-Version']='0.2.0'
            self.assertEqual(check_client(root,h),(426,""))
    def test_invalid_policy_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);p=root/'client-policy.json'
            for content in ('{','{}','x'*4097):
                p.write_text(content,encoding='utf-8')
                with self.assertRaises(ValueError):check_client(root,Message())
                self.assertEqual(p.read_text(encoding='utf-8-sig'),content)
