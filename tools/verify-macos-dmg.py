"""Mount the final DMG read-only and compare its actual signed application."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from importlib.util import module_from_spec, spec_from_file_location

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--dmg',type=Path,required=True);parser.add_argument('--app',type=Path,required=True);parser.add_argument('--result',type=Path,required=True);args=parser.parse_args()
    spec=spec_from_file_location('mac_audit',Path(__file__).with_name('verify-macos.py'));audit=module_from_spec(spec);spec.loader.exec_module(audit)
    with tempfile.TemporaryDirectory(prefix='TeamHJD-dmg-') as temp:
        mount=Path(temp)/'mounted';mount.mkdir();attached=False
        try:
            subprocess.run(['hdiutil','attach','-readonly','-nobrowse','-mountpoint',str(mount),str(args.dmg.resolve())],check=True,timeout=120,capture_output=True);attached=True
            app=mount/'TeamHJDLauncher.app'
            assert (mount/'Applications').is_symlink() and (mount/'Applications').readlink()==Path('/Applications')
            assert audit.tree_files(app)==audit.tree_files(args.app.resolve()),'Mounted application differs from the audited app'
            subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True,timeout=90,capture_output=True)
            assert (mount/'legal/TERMS.ko.txt').is_file() and (mount/'legal/PRIVACY.ko.txt').is_file()
        finally:
            if attached:subprocess.run(['hdiutil','detach',str(mount)],check=True,timeout=60,capture_output=True)
    args.result.write_text(json.dumps({'result':'PASS','readOnlyMount':True,'applicationBytesAndModesMatch':True,'nativeStrictCodeSignature':True,'applicationsShortcut':True,'termsAndPrivacyIncluded':True}),encoding='utf-8')
    print('PASS final DMG read-only mount, application bytes/modes, strict signature and installation documents')
if __name__=='__main__':main()
