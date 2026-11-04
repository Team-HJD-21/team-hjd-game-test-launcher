"""Fetch/verify the pinned official NSIS distribution inside a local cache."""
import argparse
import hashlib
import html
import io
from pathlib import Path, PurePosixPath
import re
import urllib.parse
import urllib.request
import zipfile

VERSION='3.13'
SHA256='ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'
SIZE=2363925
URL='https://sourceforge.net/projects/nsis/files/NSIS%203/3.13/nsis-3.13.zip/download'

def official(url):
    u=urllib.parse.urlsplit(url)
    if u.scheme!='https' or u.username or u.password or u.hostname is None or not (u.hostname=='sourceforge.net' or u.hostname.endswith('.sourceforge.net')):
        raise ValueError('NSIS download must remain on the official HTTPS distribution')

class OfficialRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,request,fp,code,message,headers,new_url):
        official(new_url)
        return super().redirect_request(request,fp,code,message,headers,new_url)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cache',type=Path,required=True);parser.add_argument('--offline',action='store_true');args=parser.parse_args()
    cache=args.cache.absolute()
    for path in (cache,*cache.parents):
        if path.is_symlink() or (hasattr(path,'is_junction') and path.is_junction()):raise ValueError('Linked tool cache refused')
    archive=cache/f'nsis-{VERSION}-verified.zip'
    if archive.is_symlink() or (hasattr(archive,'is_junction') and archive.is_junction()):raise ValueError('Linked NSIS archive refused')
    if archive.exists():
        if archive.stat().st_size!=SIZE:raise ValueError('Cached NSIS archive has an unexpected size')
        data=archive.read_bytes()
    else:
        if args.offline:raise ValueError('The verified official NSIS archive is not cached')
        address=URL
        for _ in range(4):
            official(address)
            with urllib.request.build_opener(OfficialRedirects()).open(address,timeout=60) as response:
                official(response.url);data=response.read(8*1024*1024+1)
            if data.startswith(b'PK\x03\x04'):break
            page=data.decode('utf-8',errors='strict')
            refresh=re.search(r'<meta\b[^>]*http-equiv=["\']refresh["\'][^>]*content=["\'][^"\']*?url=([^"\']+)',page,re.I)
            if not refresh:raise ValueError('Official NSIS download returned HTML without a supported redirect')
            address=html.unescape(refresh[1]);official(address)
        else:raise ValueError('Official NSIS download did not produce a ZIP')
    if len(data)!=SIZE or hashlib.sha256(data).hexdigest()!=SHA256:raise ValueError('Official NSIS archive size/hash differs; no tool was executed')
    cache.mkdir(parents=True,exist_ok=True)
    if not archive.exists():
        with archive.open('xb') as output:output.write(data)
    with zipfile.ZipFile(io.BytesIO(data)) as package:
        for entry in package.infolist():
            name=PurePosixPath(entry.filename)
            if name.is_absolute() or '..' in name.parts or '\\' in entry.filename or not name.parts or name.parts[0]!=f'nsis-{VERSION}':raise ValueError('Unsafe NSIS archive path')
            target=cache.joinpath(*name.parts)
            for parent in (target,*target.parents):
                if parent==cache.parent:break
                if parent.is_symlink() or (hasattr(parent,'is_junction') and parent.is_junction()):raise ValueError('Linked extracted tool refused')
            if entry.is_dir():target.mkdir(parents=True,exist_ok=True);continue
            content=package.read(entry)
            if target.exists():
                if target.read_bytes()!=content:raise ValueError('Cached NSIS file differs from its pinned archive; choose a fresh tool cache')
            else:
                target.parent.mkdir(parents=True,exist_ok=True)
                with target.open('xb') as output:output.write(content)
    print(f'PASS official NSIS {VERSION} archive and extracted bytes verified: {SHA256}')
if __name__=='__main__':main()
