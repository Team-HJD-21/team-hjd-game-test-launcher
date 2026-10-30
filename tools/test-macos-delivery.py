"""Native CI acceptance: unpacked GUI, stable bootstrap, activation and rollback."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time


def load(path):
    return json.loads(path.read_text())


def save(path, data):
    path.write_text(json.dumps(data))


def actual_path(pid):
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    buffer = ctypes.create_string_buffer(4096)
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    count = library.proc_pidpath(pid, buffer, len(buffer))
    return Path(os.fsdecode(buffer.value)).resolve() if count > 0 else None


def stop_owned(root):
    state = load(root / 'launcher/running.json')
    expected = (root / 'launcher/versions' / state['folder'] / state['executable']).resolve()
    pid = state['pid']
    assert actual_path(pid) == expected, 'Unexpected child process'
    os.kill(pid, signal.SIGTERM)
    for _ in range(100):
        if actual_path(pid) != expected:
            return
        time.sleep(.1)
    raise AssertionError('The acceptance child did not exit')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--roundtrip', type=Path, required=True)
    parser.add_argument('--arch', choices=['arm64', 'x64'], required=True)
    args = parser.parse_args()
    output = args.app.resolve().parents[2]
    active = load(args.roundtrip / 'launcher/active.json')['current']
    extracted = args.roundtrip / 'launcher/versions' / active
    executable = 'TeamHJDClient.app/Contents/MacOS/TeamHJDClient'
    subprocess.run([str(extracted / executable), '--data-root', str(args.roundtrip / 'ui-profile'),
                    '--capture', str(output / 'macos-ui.png'), '--quit-after-ms', '3400'], check=True, timeout=25)
    assert (output / 'macos-ui.png').stat().st_size > 10_000
    runtime = Path.home() / 'Library/Application Support/TeamHJD' / ('LauncherRuntime-macos-' + args.arch)
    assert not runtime.exists(), 'Bootstrap fixture requires a clean disposable runner'
    bootstrap = args.app.resolve() / 'Contents/MacOS/TeamHJDLauncher'

    def launch():
        subprocess.run([str(bootstrap), '--noninteractive-test'], check=True, timeout=40)
        state = load(runtime / 'launcher/running.json')
        assert actual_path(state['pid']) == (runtime / 'launcher/versions' / state['folder'] / executable).resolve()
        assert not (runtime / 'pending-launcher.json').exists()
        return load(runtime / 'launcher/active.json')

    seed = launch()
    stop_owned(runtime)
    target = runtime / 'launcher/versions' / active
    shutil.copytree(extracted, target)
    save(runtime / 'queued-launcher.json', {'candidate': active})
    applied = launch()
    assert applied['current'] == active and applied['previous'] == seed['current']
    stop_owned(runtime)
    bad = '0.1.3-bad-native-fixture'
    rejected = runtime / 'launcher/versions' / bad
    shutil.copytree(extracted, rejected)
    # A fully receipted synthetic native process that never sends a frame ACK.
    # This tests the health check independently from hash/corruption rejection.
    fixture = Path('build-macos/LauncherFixtureGame').resolve()
    shutil.copy2(fixture, rejected / executable)
    manifest = load(rejected / '._teamhjd-receipt.json')
    for file in manifest['files']:
        if file['path'] == executable:
            file['bytes'] = fixture.stat().st_size
            file['sha256'] = hashlib.sha256(fixture.read_bytes()).hexdigest()
    save(rejected / '._teamhjd-receipt.json', manifest)
    save(runtime / 'queued-launcher.json', {'candidate': bad})
    recovered = launch()
    assert recovered['current'] == active, 'Failed candidate did not roll back'
    stop_owned(runtime)
    result = {'result': 'PASS', 'arch': args.arch, 'nativePackagedGUI': True,
              'nativeBootstrapSeedAndFrameAck': True, 'queuedPayloadActivation': True,
              'failedNativeHealthRollback': True, 'humanEmailOTPTested': False,
              'unityGameOrCrossPlatformMultiplayerTested': False}
    save(output / 'delivery-result.json', result)
    print('PASS native unpacked GUI, initial install, queued activation and failed-health rollback')


if __name__ == '__main__':
    main()
