import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('mac_audit',Path(__file__).resolve().parents[1]/'tools/verify-macos.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
class MinimumTests(unittest.TestCase):
    def test_actual_helper_style_metadata(self):
        self.assertEqual(audit.component_minimum(['Load command 7','cmd LC_BUILD_VERSION','cmdsize 24','platform 1','minos 15.0','sdk 15.4','ntools 0']),(15,0,0))
    def test_old_and_multiple_architecture_commands(self):
        self.assertEqual(audit.component_minimum(['Load command 1','cmd LC_VERSION_MIN_MACOSX','version 12.0.1','sdk 15.0','Load command 2','cmd LC_BUILD_VERSION','minos 15.0.0','sdk 26.0']),(15,0,0))
    def test_missing_or_invalid_minimum_rejected(self):
        for lines in (['cmd LC_BUILD_VERSION','sdk 26.0'],['cmd LC_BUILD_VERSION','minos invalid']):
            with self.assertRaises(ValueError):audit.component_minimum(lines)
