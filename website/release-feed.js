'use strict';
// Same pinned public key and immutable asset contract as the native updater.
// No API token, session cookie, email or private game endpoint is used here.
globalThis.TeamHJDFeed = (() => {
  const keyId = 'launcher-p256-2026-11';
  const publicKey = 'c1bEN1h9jmxaT3QdMpSDH8hqNzs5aU+P0gpxpdX3FEXJxpsdv/pfc40r6SCSFkmK0IEoAo0YkfZ6iKYi9brnYQ==';
  const decode = text => Uint8Array.from(atob(text), c => c.charCodeAt(0));
  const version = s => typeof s === 'string' && /^(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})\.(0|[1-9][0-9]{0,3})$/.test(s);
  const compare = (a,b) => { const x=a.split('.').map(Number), y=b.split('.').map(Number); for(let i=0;i<3;i++){if(x[i]!==y[i])return x[i]-y[i]} return 0; };
  async function verify(envelope, minimum=0, now=Date.now()) {
    if (!envelope || envelope.keyId !== keyId || typeof envelope.payload !== 'string' || envelope.payload.length > 256*1024 || typeof envelope.signature !== 'string') throw Error('Invalid release envelope');
    const bytes=decode(envelope.payload), signature=decode(envelope.signature);
    const point=new Uint8Array(65);point[0]=4;point.set(decode(publicKey),1);
    const key=await crypto.subtle.importKey('raw',point,{name:'ECDSA',namedCurve:'P-256'},false,['verify']);
    if(signature.length!==64 || !await crypto.subtle.verify({name:'ECDSA',hash:'SHA-256'},key,signature,bytes))throw Error('Invalid release signature');
    return validate(JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes)),minimum,now);
  }
  function validate(p, minimum, now) {
    if(!p || p.schema!==1 || !Number.isSafeInteger(p.sequence) || p.sequence<1 || p.sequence<minimum || !version(p.version) || !version(p.minimumVersion) || compare(p.minimumVersion,p.version)>0)throw Error('Invalid release version');
    const at=Date.parse(p.publishedAt), end=Date.parse(p.expiresAt);
    if(typeof p.publishedAt!=='string' || !p.publishedAt.endsWith('Z') || typeof p.expiresAt!=='string' || !p.expiresAt.endsWith('Z') || !Number.isFinite(at) || !Number.isFinite(end) || at>now+300000 || end<=now || end<=at || end-at>366*86400000)throw Error('Expired release');
    for(const platform of ['windows-x64','macos-arm64','macos-x64']) {
      const suffix=platform==='windows-x64'?'Windows-x64':'macOS-'+platform.slice(6);
      for(const [kind,name] of [['payload',`TeamHJD-LauncherPayload-${suffix}.zip`],['installer',`TeamHJD-GameTestLauncher-${suffix}${platform==='windows-x64'?'-Setup.exe':'.dmg'}`]]) {
        const a=p.platforms?.[platform]?.[kind];
        if(!a || a.name!==name || a.url!==`https://github.com/Team-HJD-21/team-hjd-game-test-launcher/releases/download/v${p.version}/${name}` || !Number.isSafeInteger(a.bytes) || a.bytes<1 || a.bytes>=2**31 || !/^[a-f0-9]{64}$/.test(a.sha256))throw Error('Invalid release artifact');
      }
    }
    return p;
  }
  return {verify};
})();
