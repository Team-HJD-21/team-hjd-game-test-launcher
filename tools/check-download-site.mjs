import assert from 'node:assert/strict';
import {readFileSync, readdirSync, lstatSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../website');
const expectedFiles = new Set(['.nojekyll', 'CNAME', 'robots.txt', 'index.html', 'styles.css', 'app.js', 'assets/teamhjd-logo.png', 'assets/teamhjd-wordmark.png']);
function files(directory, prefix = '') {
  return readdirSync(directory).flatMap(name => {
    const file = path.join(directory, name);
    assert(!lstatSync(file).isSymbolicLink(), 'Public site must not contain symlinks');
    return lstatSync(file).isDirectory() ? files(file, prefix + name + '/') : [prefix + name];
  });
}
assert.deepEqual(new Set(files(root)), expectedFiles, 'Only reviewed static public files may be deployed');
assert.equal(readFileSync(path.join(root, 'CNAME'), 'utf8').trim(), 'testlauncher.teamhjd.com');
const html = readFileSync(path.join(root, 'index.html'), 'utf8');
const app = readFileSync(path.join(root, 'app.js'), 'utf8');
const expectedAssets = ['Windows-x64', 'macOS-arm64', 'macOS-x64'].map(platform => `TeamHJD-GameTestLauncher-${platform}.zip`);
const downloadLinks = [...html.matchAll(/data-download="[^"]+" href="([^"]+)"/g)].map(match => match[1]);
assert.deepEqual(new Set(downloadLinks), new Set(expectedAssets.map(name => `https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/latest/download/${name}`)));
assert.equal(downloadLinks.length, 3, 'Exactly three portable launcher downloads');
assert(!html.includes('LauncherPayload-') && !html.includes('playtest.teamhjd.com'), 'No updater payload or retired private game portal links');
assert(!/releases\/download\/v[0-9]/.test(html), 'Download links must not pin an old release');
assert(/name="robots" content="noindex,nofollow"/.test(html), 'Unlisted page must request no indexing');
assert(/credentials: 'omit'/.test(app), 'Public GitHub metadata never needs a user token');
assert(!/innerHTML|document\.write|eval\(/.test(app), 'External metadata must not become executable markup');
assert(!/<(?:iframe|form)\b/i.test(html), 'No embedded login, upload or data collection');
for (const url of [...html.matchAll(/(?:href|src)="([^"]+)"/g)].map(match => match[1])) {
  if (/^(?:https:|#)/.test(url)) continue;
  assert(!url.includes('..') && expectedFiles.has(url), `Unknown or unsafe local resource: ${url}`);
}
console.log('PASS public-site file boundary, unlisted metadata, three stable latest portable links and token-free metadata rendering');
