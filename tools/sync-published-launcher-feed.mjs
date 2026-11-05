// CI consumes an already signed public asset; publisher private keys stay local.
import {readFileSync,writeFileSync} from 'node:fs';
import {webcrypto,createHash} from 'node:crypto';
import vm from 'node:vm';
const root=new URL('../',import.meta.url);
const repository='Team-HJD-21/team-hjd-game-test-launcher';
const context={crypto:webcrypto,Uint8Array,TextDecoder,atob};vm.createContext(context);
vm.runInContext(readFileSync(new URL('website/release-feed.js',root),'utf8'),context);
const current=await context.TeamHJDFeed.verify(JSON.parse(readFileSync(new URL('website/updates/stable.json',root),'utf8')));
// Preserve the deployed monotonic baseline even when a previous publication
// only updated Pages artifacts and did not write its feed back into Git.
const liveResponse=await fetch('https://testlauncher.teamhjd.com/updates/stable.json?sync='+Date.now(),{signal:AbortSignal.timeout(20000)});
if(!liveResponse.ok||Number(liveResponse.headers.get('content-length'))>256*1024)throw new Error('Cannot verify the currently deployed feed baseline');
const liveText=await liveResponse.text();if(liveText.length>256*1024)throw new Error('Deployed feed exceeds the limit');
const live=await context.TeamHJDFeed.verify(JSON.parse(liveText));
const minimumSequence=Math.max(current.sequence,live.sequence);
const metadataResponse=await fetch(`https://api.github.com/repos/${repository}/releases/latest`,{headers:{Accept:'application/vnd.github+json','User-Agent':'TeamHJD-signed-feed-publisher'},signal:AbortSignal.timeout(20000)});
if(!metadataResponse.ok)throw new Error('Cannot read the published stable release');
const release=await metadataResponse.json();
if(release.draft||release.prerelease||release.immutable!==true||!/^v\d+\.\d+\.\d+$/.test(release.tag_name)||!Array.isArray(release.assets)||release.assets.length!==11)throw new Error('Expected a complete immutable stable release');
const url=`https://github.com/${repository}/releases/download/${release.tag_name}/launcher-stable.json`;
const asset=release.assets.filter(file=>file.name==='launcher-stable.json');
if(asset.length!==1||asset[0].state!=='uploaded'||asset[0].browser_download_url!==url||asset[0].size>256*1024)throw new Error('Invalid feed asset metadata');
const response=await fetch(url,{signal:AbortSignal.timeout(30000)});
if(!response.ok||Number(response.headers.get('content-length'))>256*1024)throw new Error('Cannot retrieve the public signed feed');
const bytes=Buffer.from(await response.arrayBuffer());
if(bytes.length>256*1024||bytes.length!==asset[0].size||'sha256:'+createHash('sha256').update(bytes).digest('hex')!==asset[0].digest)throw new Error('Public feed asset size or digest differs');
const candidate=await context.TeamHJDFeed.verify(JSON.parse(bytes.toString('utf8')),minimumSequence);
if('v'+candidate.version!==release.tag_name||!/^([a-f0-9]{40})$/.test(candidate.sourceCommit))throw new Error('Signed feed does not match the published release');
if(candidate.sequence===current.sequence&&JSON.stringify(candidate)!==JSON.stringify(current))throw new Error('Conflicting signed contents at the same sequence');
if(candidate.sequence===live.sequence&&JSON.stringify(candidate)!==JSON.stringify(live))throw new Error('Published feed conflicts with the deployed sequence');
for(const platform of Object.values(candidate.platforms))for(const file of Object.values(platform)){
  const match=release.assets.filter(asset=>asset.name===file.name);
  if(match.length!==1||match[0].state!=='uploaded'||match[0].browser_download_url!==file.url||match[0].size!==file.bytes||match[0].digest!=='sha256:'+file.sha256)throw new Error('Published installer/payload differs from the signed feed');
}
writeFileSync(new URL('website/updates/stable.json',root),bytes);
console.log(`PASS synchronized verified public feed ${candidate.version}, sequence ${candidate.sequence}; all six signed asset digests match`);
