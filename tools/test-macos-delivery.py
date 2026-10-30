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
import uuid


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


def prepare_failed_health_fixture(rejected, executable, fixture):
    target = rejected / executable
    shutil.copy2(fixture, target)
    target.chmod(0o755)
    flag = rejected / 'no-health.flag'
    flag.write_text('Deliberately exit before writing a bootstrap health acknowledgment.\n')
    flag.chmod(0o644)
    manifest = load(rejected / '._teamhjd-receipt.json')
    assert not any(file['path'] == flag.name for file in manifest['files']), 'Unexpected preexisting no-health fixture entry'
    found = False
    for file in manifest['files']:
        if file['path'] == executable:
            found = True
            file.update(bytes=target.stat().st_size,
                        sha256=hashlib.sha256(target.read_bytes()).hexdigest(), executableFile=True)
    assert found, 'The failed-health fixture must replace a receipted client executable'
    manifest['files'].append({'path': flag.name, 'bytes': flag.stat().st_size,
                              'sha256': hashlib.sha256(flag.read_bytes()).hexdigest(), 'executableFile': False})
    manifest['expandedBytes'] = sum(file['bytes'] for file in manifest['files'])
    save(rejected / '._teamhjd-receipt.json', manifest)
    probe = rejected.parent / ('no-health-probe-' + uuid.uuid4().hex + '.json')
    assert not probe.exists(), 'A health probe must start without a stale acknowledgment'
    result = subprocess.run([str(target), '--health-file', str(probe)], cwd=rejected, check=False, timeout=10)
    assert result.returncode == 4, 'The failed-health fixture did not take its deliberate exit path'
    assert not probe.exists(), 'The failed-health fixture unexpectedly acknowledged healthy startup'
    print('PASS failed-health fixture exits with code 4 without an acknowledgment', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--roundtrip', type=Path, required=True)
    parser.add_argument('--arch', choices=['arm64', 'x64'], required=True)
    args = parser.parse_args()
    print('Checking unpacked native GUI', flush=True)
    output = args.app.resolve().parents[2]
    active = load(args.roundtrip / 'launcher/active.json')['current']
    extracted = args.roundtrip / 'launcher/versions' / active
    executable = 'TeamHJDClient.app/Contents/MacOS/TeamHJDClient'
    subprocess.run([str(extracted / executable), '--data-root', str(args.roundtrip / 'ui-profile'),
                    '--capture', str(output / 'macos-ui.png'), '--quit-after-ms', '3400'], check=True, timeout=25)
    assert (output / 'macos-ui.png').stat().st_size > 10_000
    runtime = Path.home() / 'Library/Application Support/TeamHJD' / ('LauncherRuntime-macos-' + args.arch)
    assert not runtime.exists(), 'Bootstrap fixture requires a clean disposable runner'
    partial = runtime / 'launcher/versions/0.1.3-seed'
    partial.mkdir(parents=True)
    (partial / 'interrupted-copy.txt').write_text('Incomplete prior first-use attempt, retain for diagnosis.')
    bootstrap = args.app.resolve() / 'Contents/MacOS/TeamHJDLauncher'

    def launch():
        subprocess.run([str(bootstrap), '--noninteractive-test'], check=True, timeout=40)
        state = load(runtime / 'launcher/running.json')
        assert actual_path(state['pid']) == (runtime / 'launcher/versions' / state['folder'] / executable).resolve()
        assert not (runtime / 'pending-launcher.json').exists()
        return load(runtime / 'launcher/active.json')

    seed = launch()
    print('PASS initial bootstrap seed and first-frame acknowledgment', flush=True)
    stop_owned(runtime)
    target = runtime / 'launcher/versions' / active
    shutil.copytree(extracted, target)
    save(runtime / 'queued-launcher.json', {'candidate': active})
    applied = launch()
    assert applied['current'] == active and applied['previous'] == seed['current']
    print('PASS queued native launcher payload activation', flush=True)
    stop_owned(runtime)
    bad = '0.1.3-bad-native-fixture'
    rejected = runtime / 'launcher/versions' / bad
    shutil.copytree(extracted, rejected)
    # A fully receipted synthetic native process that never sends a frame ACK.
    # This tests the health check independently from hash/corruption rejection.
    fixture = Path('build-macos/LauncherFixtureGame').resolve()
    prepare_failed_health_fixture(rejected, executable, fixture)
    save(runtime / 'queued-launcher.json', {'candidate': bad})
    recovered = launch()
    assert recovered['current'] == active, 'Failed candidate did not roll back'
    stop_owned(runtime)
    result = {'result': 'PASS', 'arch': args.arch, 'nativePackagedGUI': True,
              'interruptedInitialCopyRecovery': True,
              'nativeBootstrapSeedAndFrameAck': True, 'queuedPayloadActivation': True,
              'failedNativeFixtureExitWithoutAck': True, 'failedNativeHealthRollback': True, 'humanEmailOTPTested': False,
              'unityGameOrCrossPlatformMultiplayerTested': False}
    save(output / 'delivery-result.json', result)
    print('PASS native unpacked GUI, initial install, queued activation and failed-health rollback')


if __name__ == '__main__':
    main()
