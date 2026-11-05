// Render public GitHub release notes as inert HTML; no client API or credentials.
import {readFileSync, writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const repository='Team-HJD-21/team-hjd-game-test-launcher';
const escape=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function inline(value) {
  return escape(value).replace(/\[([^\]\n]+)\]\((https:\/\/[^\s)]+)\)/g,(_,label,url)=>`<a href="${url}" rel="noopener noreferrer">${label}</a>`)
    .replace(/\*\*([^*\n]+)\*\*/g,'<strong>$1</strong>').replace(/`([^`\n]+)`/g,'<code>$1</code>');
}
function body(value) {
  return value.replace(/\r/g,'').trim().split(/\n\s*\n/).filter(Boolean).map(block=>{
    const lines=block.split('\n');
    if(lines.every(line=>/^[-*] /.test(line)))return '<ul>'+lines.map(line=>'<li>'+inline(line.slice(2))+'</li>').join('')+'</ul>';
    return '<p>'+lines.map(line=>inline(line.replace(/^#{1,6} /,''))).join('<br>')+'</p>';
  }).join('\n');
}
const fixtureIndex=process.argv.indexOf('--source');
let releases=[];
if(fixtureIndex>=0)releases=JSON.parse(readFileSync(process.argv[fixtureIndex+1],'utf8').replace(/^\uFEFF/,''));
else {
  for(let page=1;page<=10;page++) {
    const response=await fetch(`https://api.github.com/repos/${repository}/releases?per_page=100&page=${page}`,{
      headers:{Accept:'application/vnd.github+json','User-Agent':'TeamHJD-public-release-history'},signal:AbortSignal.timeout(20000)});
    if(!response.ok)throw new Error('Public release history HTTP '+response.status);
    const batch=await response.json();if(!Array.isArray(batch))throw new Error('Invalid public release list');
    releases.push(...batch);if(batch.length<100)break;if(page===10)throw new Error('Release pagination exceeds reviewed limit');
  }
}
releases=releases.filter(release=>!release.draft && release.published_at).sort((a,b)=>Date.parse(b.published_at)-Date.parse(a.published_at));
if(!releases.length)throw new Error('Refusing to replace history with an empty response');
const seen=new Set();
const entries=releases.map(release=>{
  if(!/^v\d+\.\d+\.\d+(?:[-.][A-Za-z0-9.-]+)?$/.test(release.tag_name)||seen.has(release.tag_name))throw new Error('Invalid or duplicate public release tag');
  seen.add(release.tag_name);
  const url=`https://github.com/${repository}/releases/tag/${release.tag_name}`;
  if(release.html_url!==url||!Number.isFinite(Date.parse(release.published_at))||typeof release.body!=='string'||release.body.length>100000)throw new Error('Invalid release metadata');
  return `<article class="release-entry" id="${escape(release.tag_name)}"><header><h2>${escape(release.name||release.tag_name)}</h2><div class="release-meta"><time datetime="${escape(release.published_at)}">${release.published_at.slice(0,10)}</time><a href="${url}">GitHub Release</a></div></header><div class="release-notes">${body(release.body)}</div></article>`;
}).join('\n');
const target=path.join(root,'website/releases.html');
const source=readFileSync(target,'utf8');
const start='<!-- release-history:start -->',end='<!-- release-history:end -->';
if(source.split(start).length!==2||source.split(end).length!==2)throw new Error('Expected one history insertion boundary');
writeFileSync(target,source.slice(0,source.indexOf(start)+start.length)+'\n'+entries+'\n'+source.slice(source.indexOf(end)),'utf8');
console.log(`PASS rendered ${entries.length ? releases.length : 0} public releases (${releases[0].tag_name} first)`);
