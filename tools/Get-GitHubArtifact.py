"""Receive a verified final Mac artifact using bounded authenticated-API ranges.

GitHub credentials go only to api.github.com. Signed storage URLs stay in memory.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile

REPO='Team-HJD-21/team-hjd-game-test-launcher'
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args):return None
def github(route):
    return json.loads(subprocess.run(['gh','api','repos/'+REPO+route],check=True,capture_output=True,text=True,encoding='utf-8',timeout=60).stdout)
def main():
    p=argparse.ArgumentParser();p.add_argument('--artifact-id',type=int,required=True);p.add_argument('--run-id',type=int,required=True);p.add_argument('--source-sha',required=True);p.add_argument('--arch',choices=['arm64','x64'],required=True);p.add_argument('--target',type=Path,required=True);a=p.parse_args()
    assert a.artifact_id>0 and a.run_id>0 and re.fullmatch('[a-f0-9]{40}',a.source_sha)
    metadata=github('/actions/artifacts/'+str(a.artifact_id));run=github('/actions/runs/'+str(a.run_id))
    assert metadata['name']=='macOS-'+a.arch and not metadata['expired']
    assert metadata['workflow_run']['id']==a.run_id and metadata['workflow_run']['head_sha']==a.source_sha
    assert run['head_sha']==a.source_sha and run['conclusion']=='success' and run['repository']['full_name']==REPO
    assert re.fullmatch('sha256:[0-9a-f]{64}',metadata['digest'])
    size=metadata['size_in_bytes'];assert 0<size<1024**3
    target=a.target.absolute();assert not target.exists()
    token=subprocess.run(['gh','auth','token'],check=True,capture_output=True,text=True,timeout=30).stdout.strip()
    request=urllib.request.Request(f'https://api.github.com/repos/{REPO}/actions/artifacts/{a.artifact_id}/zip',headers={'Authorization':'Bearer '+token,'User-Agent':'TeamHJD-Release-Artifact','Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','Cache-Control':'no-cache'})
    try:
        response=urllib.request.build_opener(NoRedirect).open(request,timeout=30);response.close();raise ValueError('Expected official archive redirect')
    except urllib.error.HTTPError as error:
        assert error.code==302;location=error.headers['Location'];error.close()
    del token,request
    url=urllib.parse.urlsplit(location)
    assert url.scheme=='https' and url.hostname.endswith('.blob.core.windows.net') and not url.username and not url.password
    root=Path(__file__).resolve().parents[1]/'.local'/('artifact-ranges-'+str(a.artifact_id)+'-'+uuid.uuid4().hex[:8]);root.mkdir()
    chunk=2*1024*1024;spans=[(n,min(n+chunk-1,size-1)) for n in range(0,size,chunk)]
    def fetch(index):
        start,end=spans[index]
        req=urllib.request.Request(location,headers={'Range':f'bytes={start}-{end}','User-Agent':'TeamHJD-Release-Artifact'})
        with urllib.request.build_opener(NoRedirect).open(req,timeout=90) as response:
            assert response.status==206 and response.headers['Content-Range']==f'bytes {start}-{end}/{size}'
            data=response.read(end-start+2);assert len(data)==end-start+1
        (root/f'{index:04d}.part').write_bytes(data)
        return len(data)
    received=0
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        futures=[pool.submit(fetch,i) for i in range(len(spans))]
        for n,future in enumerate(concurrent.futures.as_completed(futures),1):
            received+=future.result()
            if n%16==0 or n==len(spans):print(f'Validated bytes {received}/{size}',flush=True)
    archive=root/('macOS-'+a.arch+'-artifact.zip');checksum=hashlib.sha256()
    with archive.open('xb') as output:
        for index in range(len(spans)):
            data=(root/f'{index:04d}.part').read_bytes();checksum.update(data);output.write(data)
    assert archive.stat().st_size==size and 'sha256:'+checksum.hexdigest()==metadata['digest']
    names={'audit-result.json','delivery-result.json','dmg-result.json','native-source-result.json','macos-ui.png',f'TeamHJD-GameTestLauncher-macOS-{a.arch}.zip',f'TeamHJD-LauncherPayload-macOS-{a.arch}.zip',f'TeamHJD-GameTestLauncher-macOS-{a.arch}.dmg'}
    with zipfile.ZipFile(archive) as source:
        assert set(source.namelist())==names and len(source.infolist())==len(names) and source.testzip() is None
        target.mkdir(parents=True,exist_ok=False)
        for name in sorted(names):(target/name).write_bytes(source.read(name))
    proof={'result':'PASS','artifactId':a.artifact_id,'runId':a.run_id,'sourceSha':a.source_sha,'bytes':size,'digest':metadata['digest'],'validatedRanges':len(spans),'wholeArchiveDigestVerified':True}
    (root/'download-result.json').write_text(json.dumps(proof,indent=2),encoding='utf-8')
    print(json.dumps(proof),flush=True)
if __name__=='__main__':
    try:main()
    except Exception as error:
        print('Artifact download failed: '+type(error).__name__+'; partial data was not accepted.',file=sys.stderr);raise SystemExit(1)
