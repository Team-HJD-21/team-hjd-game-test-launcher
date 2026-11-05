"""Include the official SDK/DXC redistribution notices; never execute DXC."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.parse
import urllib.request
import zipfile

URL='https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip'
SHA256='01d4c4dfa37dee21afe70cac510d63001b6b611a128e3760f168765eead1e625'
NAMES=('LICENSE-LLVM.txt','LICENSE-MIT.txt','LICENSE-MS.txt')
class OfficialRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,request,fp,code,message,headers,new_url):
        url=urllib.parse.urlsplit(new_url)
        if url.scheme!='https' or url.hostname not in ('github.com','release-assets.githubusercontent.com','objects.githubusercontent.com') or url.username or url.password:raise ValueError('Unexpected official license download redirect')
        return super().redirect_request(request,fp,code,message,headers,new_url)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--sdk',type=Path,required=True);args=parser.parse_args()
    cache=Path(__file__).resolve().parents[1]/'.deps/dxc-license-source';cache.mkdir(parents=True,exist_ok=True)
    source=cache/'dxc_2023_08_14.zip'
    if not source.exists():
        with urllib.request.build_opener(OfficialRedirects()).open(URL,timeout=60) as response:data=response.read(32*1024*1024+1)
        if hashlib.sha256(data).hexdigest()!=SHA256:raise ValueError('Official DXC license-source hash mismatch')
        with source.open('xb') as output:output.write(data)
    if source.is_symlink() or hashlib.sha256(source.read_bytes()).hexdigest()!=SHA256:raise ValueError('Cached DXC license-source hash mismatch')
    destination=args.output/'Microsoft';destination.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(source) as archive:
        for name in NAMES:
            content=archive.read(name)
            if len(content)>1024*1024:raise ValueError('Oversized license text')
            (destination/('DXC-'+name)).write_bytes(content)
    licenses=sorted((args.sdk/'Licenses').glob('10.*/sdk_license.rtf'))
    if not licenses:raise ValueError('Installed Windows SDK redistribution license is required')
    for license in licenses:shutil.copyfile(license,destination/('Windows-SDK-'+license.parent.name+'.rtf'))
    (destination/'shader-license-provenance.json').write_text(json.dumps({'source':URL,'sourceArchiveSha256':SHA256,'licenseFiles':list(NAMES),'binaries':'Unmodified installed Windows SDK Redist/D3D/x64; see payload file receipts','windowsOnly':True},indent=2),encoding='utf-8')
    print('PASS Windows SDK and pinned official DXC redistribution notices included; no downloaded executable was run.')
if __name__=='__main__':main()
