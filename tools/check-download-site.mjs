import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, readdirSync, lstatSync} from 'node:fs';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../website');
const faviconBase = 'assets/favicons/v1/launcher/';
const favicon = JSON.parse(readFileSync(path.resolve(root, '../tools/launcher-favicon-v1.json'), 'utf8'));
const faviconNames = ['favicon.svg','favicon.ico','favicon-16.png','favicon-32.png','favicon-48.png','favicon-64.png','favicon-192.png','favicon-512.png','apple-touch-icon.png'];
assert.equal(favicon.id, 'launcher');
assert.equal(favicon.version, '1.0.0');
assert.equal(favicon.sourceCommit, '22c596ac6ea86edd5e0f7adee07875c49e0e449a');
assert.deepEqual(Object.keys(favicon.files).sort(), [...faviconNames].sort());
const expectedFiles = new Set(['.nojekyll', 'CNAME', 'robots.txt', 'index.html', 'styles.css', 'app.js', 'release-feed.js', 'privacy.html', 'assets/teamhjd-logo.png', 'assets/teamhjd-wordmark.png', 'assets/favicons/v1/.gitattributes', ...faviconNames.map(name => faviconBase + name)]);
function files(directory, prefix = '') {
  return readdirSync(directory).flatMap(name => {
    const file = path.join(directory, name);
    assert(!lstatSync(file).isSymbolicLink(), 'Public site must not contain symlinks');
    return lstatSync(file).isDirectory() ? files(file, prefix + name + '/') : [prefix + name];
  });
}
if(files(root).includes('updates/stable.json'))expectedFiles.add('updates/stable.json');
assert.deepEqual(new Set(files(root)), expectedFiles, 'Only reviewed static public files may be deployed');
assert.equal(readFileSync(path.join(root, 'CNAME'), 'utf8').trim(), 'testlauncher.teamhjd.com');
const resourceVersions = new Map(['styles.css', 'app.js', 'release-feed.js'].map(name => [name,
  createHash('sha256').update(readFileSync(path.join(root, name), 'utf8').replace(/\r\n/g, '\n')).digest('hex').slice(0, 12)]));
let html = readFileSync(path.join(root, 'index.html'), 'utf8');
if (process.argv.includes('--update-asset-versions')) {
  let updated = 0;
  html = html.replace(/((?:href|src)=")(styles\.css|app\.js|release-feed\.js)(?:\?v=[a-zA-Z0-9-]+)?(")/g, (_, prefix, name, suffix) => {
    updated++;
    return `${prefix}${name}?v=${resourceVersions.get(name)}${suffix}`;
  });
  assert.equal(updated, 3, 'Expected one stylesheet and two local scripts');
  writeFileSync(path.join(root, 'index.html'), html, 'utf8');
}
let privacy = readFileSync(path.join(root,'privacy.html'),'utf8');
if(process.argv.includes('--update-asset-versions')) {
 privacy=privacy.replace(/href="styles\.css(?:\?v=[a-zA-Z0-9-]+)?"/,`href="styles.css?v=${resourceVersions.get('styles.css')}"`);
 writeFileSync(path.join(root,'privacy.html'),privacy,'utf8');
}
assert(privacy.includes(`href="styles.css?v=${resourceVersions.get('styles.css')}"`),'Privacy stylesheet must match the versioned public stylesheet');
const footer = text => text.slice(text.indexOf('  <footer'),text.indexOf('</footer>')+9);
assert.equal(footer(privacy),footer(html),'The policy page must retain the common company footer');
assert(!/<(?:script|iframe|form)\b/i.test(privacy),'No login, script or data collection on the legal page');
assert(privacy.includes('support@teamhjd.com') && privacy.includes('180') && privacy.includes('Cloudflare'),'Current retention, processor and rights contact must be visible');
const app = readFileSync(path.join(root, 'app.js'), 'utf8');
for (const name of faviconNames) {
  const content = readFileSync(path.join(root, faviconBase, name));
  assert.equal(content.length, favicon.files[name].bytes, `Favicon size differs from upstream: ${name}`);
  assert.equal(createHash('sha256').update(content).digest('hex'), favicon.files[name].sha256, `Favicon bytes differ from upstream: ${name}`);
  if (name.endsWith('.png')) {
    assert.equal(content.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
    const size = name === 'apple-touch-icon.png' ? 180 : Number(name.match(/-(\d+)\.png$/)[1]);
    assert.equal(content.readUInt32BE(16), size);
    assert.equal(content.readUInt32BE(20), size);
  } else if (name.endsWith('.ico')) {
    assert.equal(content.readUInt16LE(0), 0);
    assert.equal(content.readUInt16LE(2), 1);
    assert.equal(content.readUInt16LE(4), 3);
    for (const [i, size] of [16, 32, 48].entries()) {
      const entry = 6 + i * 16;
      assert.equal(content[entry], size);
      assert.equal(content[entry + 1], size);
      const bytes = content.readUInt32LE(entry + 8), offset = content.readUInt32LE(entry + 12);
      assert(content.subarray(offset, offset + bytes).equals(readFileSync(path.join(root, faviconBase, `favicon-${size}.png`))), 'ICO entry must match its canonical PNG');
    }
  }
}
const iconLinks = [...html.matchAll(/<link\b[^>]*rel="(?:icon|apple-touch-icon)"[^>]*>/g)].map(match => match[0]);
assert.deepEqual(iconLinks, [
  `<link rel="icon" href="${faviconBase}favicon.ico" type="image/x-icon" sizes="16x16 32x32 48x48">`,
  `<link rel="icon" href="${faviconBase}favicon-32.png" type="image/png" sizes="32x32">`,
  `<link rel="icon" href="${faviconBase}favicon.svg" type="image/svg+xml" sizes="any">`,
  `<link rel="apple-touch-icon" href="${faviconBase}apple-touch-icon.png" sizes="180x180">`
], 'Only the assigned v1 launcher favicon family may be declared');
const expectedAssets = ['TeamHJD-GameTestLauncher-Windows-x64-Setup.exe','TeamHJD-GameTestLauncher-macOS-arm64.dmg','TeamHJD-GameTestLauncher-macOS-x64.dmg'];
const downloadLinks = [...html.matchAll(/data-download="[^"]+" href="([^"]+)"/g)].map(match => match[1]);
assert.deepEqual(new Set(downloadLinks), new Set(expectedAssets.map(name => `https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/${name}`)));
assert.equal(downloadLinks.length, 3, 'Exactly three installer downloads');
assert(!html.includes('LauncherPayload-') && !html.includes('playtest.teamhjd.com'), 'No updater payload or retired private game portal links');
assert(!/releases\/download\/v[0-9]/.test(html), 'Download links must not pin an old release');
assert(/name="robots" content="noindex,nofollow"/.test(html), 'Unlisted page must request no indexing');
assert(/credentials:\s*'omit'/.test(app), 'Public GitHub metadata never needs a user token');
assert(!/innerHTML|document\.write|eval\(/.test(app), 'External metadata must not become executable markup');
assert(!/<(?:iframe|form)\b/i.test(html), 'No embedded login, upload or data collection');
for (const url of [...html.matchAll(/(?:href|src)="([^"]+)"/g)].map(match => match[1])) {
  if (/^(?:https:|#)/.test(url)) continue;
  const resource = new URL(url, 'https://testlauncher.teamhjd.com/');
  const name = resource.pathname.slice(1);
  assert(!url.includes('..') && !url.startsWith('/') && resource.origin === 'https://testlauncher.teamhjd.com' && expectedFiles.has(name) && !resource.hash, `Unknown or unsafe local resource: ${url}`);
  assert.equal(resource.search, resourceVersions.has(name) ? `?v=${resourceVersions.get(name)}` : '', `Stale asset version: ${name}. Run node tools/check-download-site.mjs --update-asset-versions`);
}
console.log('PASS public-site boundary, canonical launcher favicons, versioned assets, latest installer links and signed token-free metadata rendering');
