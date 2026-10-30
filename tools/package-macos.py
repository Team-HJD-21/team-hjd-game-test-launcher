"""Build self-contained macOS bundles from a native CMake build. No game data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MACH = {b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
        b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca'}
HELPERS = {
    'arm64': ('arm64', '184c7d4f5f977348243a6d7ca8f6cf6073b9b2746cfe8c1fe15aa59e9a522bf0',
              '2b23964528ddcd641c1449275df8b5b51e3c7193b5257919846331bc5bc13b0f'),
    'x64': ('amd64', '6699392fed92367808c248c2d90bed66e49bbf66e1b4ee3e4141230f3f7391b3',
            'cb09afb9b4d0a4730de477ac710aebb6107568930a8345775629bf264f70c284'),
}

def run(*args):
    try:
        return subprocess.check_output([str(x) for x in args], text=True, stderr=subprocess.STDOUT,
                                       timeout=240 if Path(str(args[0])).name == 'macdeployqt' else 60)
    except subprocess.CalledProcessError as error:
        print(error.output, flush=True)
        raise

def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()

def macho(path):
    with path.open('rb') as source:
        return source.read(4) in MACH

def owned_remove(path, owner):
    assert path.resolve().is_relative_to(owner.resolve()) and path != owner
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    else:
        path.unlink()

def deploy(app, qt, arch):
    app = app.resolve(strict=True)
    print(f'Deploying Qt into {app.name}', flush=True)
    info_path = app / 'Contents/Info.plist'
    info = plistlib.loads(info_path.read_bytes())
    info.update(LSMinimumSystemVersion='12.0', NSHighResolutionCapable=True)
    info_path.write_bytes(plistlib.dumps(info))
    deploy_args = [qt / 'bin/macdeployqt', app, '-always-overwrite']
    if app.name == 'TeamHJDClient.app':
        deploy_args.append(f'-qmldir={ROOT / "qml"}')
    run(*deploy_args)
    # Explicit app-relative paths remain valid after converting frameworks into
    # ordinary dylibs; Qt must not fall back to its SDK/framework prefix.
    (app / 'Contents/Resources/qt.conf').write_text('[Paths]\nPlugins=PlugIns\nQmlImports=Resources/qml\n', encoding='utf-8')
    # macdeployqt copies optional SQL drivers that link to the SDK producer's
    # Homebrew databases. The launcher stores JSON and does not use ODBC,
    # PostgreSQL, MySQL or Mimer. Retain only the self-contained SQLite driver.
    sql_drivers = app / 'Contents/PlugIns/sqldrivers'
    if sql_drivers.is_dir():
        for driver in sql_drivers.iterdir():
            if driver.name != 'libqsqlite.dylib':
                print(f'Excluding unused SQL driver {driver.name}', flush=True)
                owned_remove(driver, app)
    frameworks = app / 'Contents/Frameworks'
    # The verified ZIP installer deliberately rejects symlinks. Convert Qt's
    # versioned frameworks to ordinary dylibs, preserving their binary resources
    # in the Qt libraries and rewriting EVERY consumer before signing.
    for framework in sorted(frameworks.glob('Qt*.framework')):
        name = framework.stem
        binary = framework / f'Versions/A/{name}'
        assert binary.is_file(), f'Missing framework binary {name}'
        shutil.copy2(binary, frameworks / f'{name}.dylib')
        owned_remove(framework, app)
    for link in sorted(app.rglob('*'), reverse=True):
        if link.is_symlink():
            target = link.resolve(strict=True)
            assert target.is_relative_to(app.resolve()), 'External deployment link'
            link.unlink()
            if target.is_dir():
                shutil.copytree(target, link, symlinks=False)
            else:
                shutil.copy2(target, link)
    files = [p for p in app.rglob('*') if p.is_file()]
    binaries = [p for p in files if macho(p)]
    print(f'Rewriting and signing {len(binaries)} native libraries in {app.name}', flush=True)
    install_names = {}
    for binary in binaries:
        # QML plugins can carry absolute SDK LC_ID_DYLIB names as well as the
        # framework libraries. Normalize every dylib ID and every consumer.
        identifiers = run('otool', '-D', binary).splitlines()[1:]
        if identifiers:
            relative = os.path.relpath(binary, frameworks)
            install_names[identifiers[0].strip()] = '@rpath/' + relative
    native_arch = 'x86_64' if arch == 'x64' else 'arm64'
    for index, binary in enumerate(binaries):
        if index % 20 == 0:
            print(f'Native library {index + 1}/{len(binaries)}', flush=True)
        arches = run('lipo', '-archs', binary).strip().split()
        assert native_arch in arches, f'Wrong architecture: {binary.name}'
        if len(arches) > 1:
            thin = binary.with_name(binary.name + '.thin')
            run('lipo', binary, '-thin', native_arch, '-output', thin)
            os.replace(thin, binary)
        references = run('otool', '-L', binary).splitlines()[1:]
        for reference in references:
            old = reference.strip().split(' (compatibility version', 1)[0]
            match = re.search(r'(Qt[^/]+)\.framework/(?:Versions/[^/]+/)?\1$', old)
            new = install_names.get(old)
            if match:
                new = f'@rpath/{match[1]}.dylib'
                assert (frameworks / f'{match[1]}.dylib').is_file()
            if new and new != old:
                run('install_name_tool', '-change', old, new, binary)
        identifiers = run('otool', '-D', binary).splitlines()[1:]
        if identifiers:
            run('install_name_tool', '-id', '@rpath/' + os.path.relpath(binary, frameworks), binary)
        relative = os.path.relpath(frameworks, binary.parent)
        rpath = '@loader_path' if relative == '.' else '@loader_path/' + relative
        existing = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', run('otool', '-l', binary))
        retained = []
        for previous in existing:
            local = previous.replace('@loader_path', str(binary.parent)).replace('@executable_path', str(app / 'Contents/MacOS'))
            if not previous.startswith(('/System/Library/', '/usr/lib/')) and (not local.startswith('/') or not Path(local).resolve().is_relative_to(app.resolve())):
                run('install_name_tool', '-delete_rpath', previous, binary)
            else:
                retained.append(previous)
        if rpath not in retained:
            run('install_name_tool', '-add_rpath', rpath, binary)
        binary.chmod(0o755)
        run('codesign', '--force', '--sign', '-', '--timestamp=none', binary)
    for path in app.rglob('*'):
        assert not path.is_symlink(), 'Deployment still contains a symlink'
        path.chmod(0o755 if path.is_dir() or path.stat().st_mode & 0o111 else 0o644)

def add_helper(client, arch, workspace):
    print(f'Downloading exact official authentication helper ({arch})', flush=True)
    upstream_arch, archive_hash, binary_hash = HELPERS[arch]
    asset = f'cloudflared-darwin-{upstream_arch}.tgz'
    archive = workspace / asset
    url = f'https://github.com/cloudflare/cloudflared/releases/download/2026.10.5/{asset}'
    with urllib.request.urlopen(url, timeout=120) as response, archive.open('wb') as output:
        shutil.copyfileobj(response, output)
    assert digest(archive) == archive_hash, 'Authentication helper archive digest mismatch'
    target = client / 'Contents/Helpers/cloudflared'
    target.parent.mkdir(parents=True)
    with tarfile.open(archive, 'r:gz') as source:
        members = [x for x in source.getmembers() if x.isfile() and x.name in ('cloudflared', './cloudflared')]
        assert len(members) == 1
        with source.extractfile(members[0]) as data, target.open('wb') as output:
            shutil.copyfileobj(data, output)
    assert digest(target) == binary_hash, 'Authentication helper executable digest mismatch'
    target.chmod(0o755)
    # Keep the exact official executable bytes: force-signing it would break the
    # native authentication helper pin. The Go binary already carries a signature.
    run('codesign', '--verify', '--strict', target)
    return {'version': '2026.10.5', 'asset': asset, 'source': url, 'sha256': binary_hash,
            'archiveSha256': archive_hash}

def receipt(directory, version):
    files = []
    for path in sorted(directory.rglob('*')):
        assert not path.is_symlink()
        if path.is_file():
            files.append({'path': path.relative_to(directory).as_posix(), 'bytes': path.stat().st_size,
                          'sha256': digest(path), 'executableFile': bool(path.stat().st_mode & 0o111)})
    return {'version': version, 'executable': 'TeamHJDClient.app/Contents/MacOS/TeamHJDClient',
            'files': files, 'expandedBytes': sum(f['bytes'] for f in files)}

def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    path.chmod(0o644)

def zip_tree(directory, output):
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(directory.rglob('*')):
            assert not path.is_symlink()
            if path.is_file():
                archive.write(path, path.relative_to(directory).as_posix())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--qt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--arch', choices=HELPERS, required=True)
    args = parser.parse_args()
    assert sys.platform == 'darwin'
    version = re.search(r'project\(TeamHJDGameTestLauncher VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text())[1]
    assert not args.output.exists(), 'Select a fresh output directory'
    args.output.mkdir(parents=True)
    workspace = args.output / 'work'
    payload = workspace / 'payload'
    portable = workspace / 'portable'
    payload.mkdir(parents=True)
    portable.mkdir()
    client = payload / 'TeamHJDClient.app'
    launcher = portable / 'TeamHJDLauncher.app'
    shutil.copytree(args.build / client.name, client, symlinks=True)
    shutil.copytree(args.build / launcher.name, launcher, symlinks=True)
    deploy(client, args.qt, args.arch)
    deploy(launcher, args.qt, args.arch)
    provenance = add_helper(client, args.arch, workspace)
    print('Signing client app and generating payload manifest', flush=True)
    run('codesign', '--force', '--sign', '-', '--timestamp=none', client)
    licenses = payload / 'licenses'
    shutil.copytree(ROOT / 'licenses/Qt', licenses / 'Qt')
    shutil.copy2(ROOT / 'vendor/miniz/LICENSE', licenses / 'miniz-LICENSE.txt')
    with urllib.request.urlopen('https://raw.githubusercontent.com/cloudflare/cloudflared/2026.10.5/LICENSE', timeout=60) as response:
        (licenses / 'cloudflared-LICENSE.txt').write_bytes(response.read())
    write_json(licenses / 'cloudflared-provenance.json', provenance)
    shutil.copy2(ROOT / 'THIRD_PARTY_NOTICES.md', payload)
    for path in payload.rglob('*'):
        path.chmod(0o755 if path.is_dir() or path.stat().st_mode & 0o111 else 0o644)
    manifest = receipt(payload, version)
    print(f'Compressing {len(manifest["files"])} payload files', flush=True)
    zip_tree(payload, args.output / f'TeamHJD-LauncherPayload-macOS-{args.arch}.zip')
    write_json(payload / '._teamhjd-receipt.json', manifest)
    shutil.copytree(payload, launcher / 'Contents/Resources/seed', symlinks=False)
    shutil.copy2(ROOT / 'docs/PLAYER_GUIDE_MACOS.ko.md', portable / '시작하기.md')
    shutil.copy2(ROOT / 'THIRD_PARTY_NOTICES.md', portable)
    run('codesign', '--force', '--sign', '-', '--timestamp=none', launcher)
    run('codesign', '--verify', '--deep', '--strict', launcher)
    print('Compressing complete portable app', flush=True)
    zip_tree(portable, args.output / f'TeamHJD-GameTestLauncher-macOS-{args.arch}.zip')
    print(f'Packaged macOS {args.arch} {version}; {len(manifest["files"])} verified payload files; no game or credentials.')

if __name__ == '__main__':
    main()
