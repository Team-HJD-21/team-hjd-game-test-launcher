"""Read-only public release checks plus disposable macOS installation probes."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
import urllib.request


def process_path(pid):
    lib = ctypes.CDLL('/usr/lib/libproc.dylib')
    buffer = ctypes.create_string_buffer(4096)
    lib.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    return Path(os.fsdecode(buffer.value)).resolve() if lib.proc_pidpath(pid, buffer, len(buffer)) > 0 else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arch', choices=['arm64', 'x64'], required=True)
    args = parser.parse_args()
    names = {'arm64': ('80524340', '1141ac50358348f5f355f527bc7cb101a3684acb589df854c4ed468a4577df52'),
             'x64': ('89622147', '47b65737671ba69af418a90ae90ad25bc8203c822622e2e80deba70d42f8dd5e')}
    runtime = Path.home() / 'Library/Application Support/TeamHJD' / ('LauncherRuntime-macos-' + args.arch)
    assert not runtime.exists(), 'A fresh disposable runner is required'
    results = []
    with tempfile.TemporaryDirectory(prefix='hjd-install-diagnostic-') as temp:
        root = Path(temp)
        disk = root / 'release.dmg'
        url = 'https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v0.2.3/TeamHJD-GameTestLauncher-macOS-' + args.arch + '.dmg'
        with urllib.request.urlopen(url, timeout=120) as response, disk.open('wb') as target:
            shutil.copyfileobj(response, target)
        assert disk.stat().st_size == int(names[args.arch][0])
        assert hashlib.sha256(disk.read_bytes()).hexdigest() == names[args.arch][1]
        mount = root / 'mounted'; mount.mkdir()
        subprocess.run(['hdiutil', 'attach', '-readonly', '-nobrowse', '-mountpoint', str(mount), str(disk)], check=True, capture_output=True, timeout=120)
        try:
            source = mount / 'TeamHJDLauncher.app'
            for method in ['ditto', 'finder', 'python']:
                folder = root / method; folder.mkdir()
                app = folder / source.name
                item = {'method': method}
                try:
                    if method == 'ditto':
                        subprocess.run(['/usr/bin/ditto', str(source), str(app)], check=True, capture_output=True, timeout=120)
                    elif method == 'finder':
                        code = 'on run argv\ntell application "Finder" to duplicate (POSIX file (item 1 of argv)) to (POSIX file (item 2 of argv))\nend run'
                        subprocess.run(['osascript', '-e', code, str(source), str(folder)], check=True, capture_output=True, timeout=45)
                    else:
                        shutil.copytree(source, app)
                    seed = app / 'Contents/Resources/seed'
                    item['seedReceiptExists'] = (seed / '._teamhjd-receipt.json').is_file()
                    original = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source.rglob('*') if p.is_file()}
                    copied = {str(p.relative_to(app)): hashlib.sha256(p.read_bytes()).hexdigest() for p in app.rglob('*') if p.is_file()}
                    item['missingFiles'] = sorted(set(original) - set(copied))
                    item['changedFiles'] = sorted(k for k in set(original) & set(copied) if original[k] != copied[k])
                    signature = subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], capture_output=True, timeout=30)
                    item['strictSignature'] = signature.returncode == 0
                    launched = subprocess.run([str(app / 'Contents/MacOS/TeamHJDLauncher'), '--noninteractive-test'], capture_output=True, text=True, timeout=45)
                    item['exitCode'] = launched.returncode
                    item['error'] = launched.stderr.strip()[-1600:]
                    state_path = runtime / 'launcher/running.json'
                    item['guiHealth'] = False
                    if state_path.exists():
                        state = json.loads(state_path.read_text())
                        expected = (runtime / 'launcher/versions' / state['folder'] / state['executable']).resolve()
                        if process_path(state['pid']) == expected:
                            item['guiHealth'] = True
                            os.kill(state['pid'], signal.SIGTERM)
                            for _ in range(100):
                                if process_path(state['pid']) != expected:
                                    break
                                time.sleep(.1)
                            assert process_path(state['pid']) != expected
                except Exception as exc:
                    item['probeError'] = str(exc)[:1000]
                finally:
                    if runtime.exists():
                        runtime.rename(runtime.with_name(runtime.name + '-diagnostic-' + method))
                results.append(item)
                print(json.dumps(item, ensure_ascii=False), flush=True)
        finally:
            subprocess.run(['hdiutil', 'detach', str(mount)], check=True, capture_output=True, timeout=60)
    Path('macos-install-diagnostic.json').write_text(json.dumps({'arch': args.arch, 'results': results}, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
