import assert from 'node:assert/strict';
import {webcrypto} from 'node:crypto';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const pair=await webcrypto.subtle.generateKey({name:'ECDSA',namedCurve:'P-256'},true,['sign','verify']);
const point=new Uint8Array(await webcrypto.subtle.exportKey('raw',pair.publicKey));
const testKey=Buffer.from(point.subarray(1)).toString('base64');
const source=readFileSync(new URL('../website/release-feed.js',import.meta.url),'utf8');
const context={crypto:webcrypto,Uint8Array,TextDecoder,atob};vm.createContext(context);
vm.runInContext(source.replace(/const publicKey = '[^']+';/,`const publicKey = '${testKey}';`),context);
const now=Date.now();let assertions=0;
const platforms={};
for(const p of ['windows-x64','macos-arm64','macos-x64']) {
 const suffix=p==='windows-x64'?'Windows-x64':'macOS-'+p.slice(6);
 const asset=name=>({name,url:`https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v0.2.0/${name}`,bytes:12345,sha256:'a'.repeat(64)});
 platforms[p]={payload:asset(`TeamHJD-LauncherPayload-${suffix}.zip`),installer:asset(`TeamHJD-GameTestLauncher-${suffix}${p==='windows-x64'?'-Setup.exe':'.dmg'}`)};
}
const valid={schema:1,sequence:2,version:'0.2.0',minimumVersion:'0.2.0',publishedAt:new Date(now-60000).toISOString(),expiresAt:new Date(now+86400000).toISOString(),platforms};
async function envelope(payload){const data=Buffer.from(JSON.stringify(payload));return {keyId:'launcher-p256-2026-11',payload:data.toString('base64'),signature:Buffer.from(await webcrypto.subtle.sign({name:'ECDSA',hash:'SHA-256'},pair.privateKey,data)).toString('base64')};}
const wire=await envelope(valid);
assert.equal((await context.TeamHJDFeed.verify(wire,1,now)).version,'0.2.0');assertions++;
await assert.rejects(()=>context.TeamHJDFeed.verify(wire,3,now));assertions++;
await assert.rejects(()=>context.TeamHJDFeed.verify({...wire,keyId:'wrong'},1,now));assertions++;
await assert.rejects(()=>context.TeamHJDFeed.verify({...wire,payload:Buffer.from('{}').toString('base64')},1,now));assertions++;
for(const change of [p=>p.version='0.2.0-beta',p=>p.minimumVersion='1.0.0',p=>p.sequence=2.5,p=>p.expiresAt=new Date(now).toISOString(),p=>p.publishedAt=new Date(now+301000).toISOString(),p=>delete p.platforms['macos-x64'],p=>p.platforms['windows-x64'].installer.url='https://example.com/setup.exe',p=>p.platforms['macos-arm64'].payload.sha256='bad']) {
 const p=structuredClone(valid);change(p);const signed=await envelope(p);await assert.rejects(()=>context.TeamHJDFeed.verify(signed,1,now));assertions++;
}
console.log(`PASS ${assertions} signed website feed assertions; ephemeral key, no publisher private key.`);
