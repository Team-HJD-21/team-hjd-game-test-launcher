"""Synthetic account fairness, release-on-error and real loopback HTTP admission."""
import base64
from contextlib import ExitStack
import hashlib
import http.client
import io
import json
from pathlib import Path
import threading
import time
from types import SimpleNamespace
import unittest
from unittest import mock
import uuid
from server.download_limits import DownloadLimits
from server import distribution_server


class LimitTests(unittest.TestCase):
    def test_account_separation_and_no_retained_identity(self):
        limits=DownloadLimits();a=b'a'*32;b=b'b'*32
        with limits.lease(a) as first, limits.lease(a) as second, limits.lease(a) as third, limits.lease(b) as other:
            self.assertEqual((first,second,third,other),(True,True,False,True))
        self.assertEqual(limits._active,{})

    def test_exception_releases_and_invalid_key_is_rejected(self):
        limits=DownloadLimits()
        with self.assertRaises(RuntimeError):
            with limits.lease(b'a'*32):raise RuntimeError('synthetic disconnect')
        self.assertEqual(limits._active,{})
        for value in (None,'email@example.invalid',b'short'):
            with self.assertRaises(ValueError):
                with limits.lease(value):pass

    def test_concurrent_same_account_cannot_race_capacity(self):
        limits=DownloadLimits();barrier=threading.Barrier(20);release=threading.Event();results=[];lock=threading.Lock()
        def attempt():
            barrier.wait()
            with limits.lease(b'a'*32) as admitted:
                with lock:results.append(admitted)
                release.wait(3)
        threads=[threading.Thread(target=attempt) for _ in range(20)]
        for thread in threads:thread.start()
        deadline=time.monotonic()+2
        while len(results)<20 and time.monotonic()<deadline:time.sleep(.01)
        release.set()
        for thread in threads:thread.join(3)
        self.assertEqual(sum(results),2);self.assertEqual(limits._active,{})


class HttpLimitTests(unittest.TestCase):
    def setUp(self):
        self.root=Path(__file__).resolve().parents[1]/'.local'/('download-limit-'+uuid.uuid4().hex)
        (self.root/'packages').mkdir(parents=True)
        self.package='/packages/'+'a'*64+'.zip'
        self.package_file=self.root/self.package[1:];self.package_file.write_bytes(b'fixture-package')
        (self.root/'catalog.json').write_text('{"fixture":true}',encoding='utf-8')
        salt=b's'*32;password=b'synthetic-owner'
        (self.root/'auth.json').write_text(json.dumps({'salt':base64.b64encode(salt).decode(),'hash':base64.b64encode(hashlib.pbkdf2_hmac('sha256',password,salt,100000)).decode(),'iterations':100000}),encoding='utf-8')
        self.release=threading.Event();ready=threading.Event();owner=self
        original_server=distribution_server.DistributionServer
        original_open=Path.open
        class Access:
            def __init__(self,root):pass
            def verify(self,token):
                if token in ('a','a-other-session','b'):
                    return SimpleNamespace(email=('b' if token=='b' else 'a')+'@example.invalid')
                return None
            def record_access(self,*args,**kwargs):pass
            def maintain_users(self,**kwargs):return None
        class Stream(io.BytesIO):
            def read(self,*args):
                if not owner.release.wait(10):raise TimeoutError('synthetic stream wait')
                return super().read(*args)
        def open_file(path,*args,**kwargs):
            if path==owner.package_file:return Stream(b'fixture-package')
            return original_open(path,*args,**kwargs)
        def create_server(*args,**kwargs):
            owner.httpd=original_server(*args,**kwargs);ready.set();return owner.httpd
        self.patches=ExitStack()
        self.patches.enter_context(mock.patch.object(distribution_server,'CloudflareAccess',Access))
        self.patches.enter_context(mock.patch.object(distribution_server,'DistributionServer',create_server))
        self.patches.enter_context(mock.patch.object(Path,'open',open_file))
        self.thread=threading.Thread(target=distribution_server.serve,args=(SimpleNamespace(root=str(self.root),bind='127.0.0.1',port=0,cert=None,key=None,test_http=True),),daemon=True)
        self.thread.start();self.assertTrue(ready.wait(5));self.port=self.httpd.server_port
        self.connections=[]

    def tearDown(self):
        self.release.set()
        for connection in self.connections:connection.close()
        self.httpd.shutdown();self.httpd.server_close();self.thread.join(3);self.patches.close()

    def request(self,token,path=None,method='GET',read=True):
        connection=http.client.HTTPConnection('127.0.0.1',self.port,timeout=5);self.connections.append(connection)
        headers={'Cf-Access-Jwt-Assertion':token} if token else {}
        connection.request(method,path or self.package,headers=headers);response=connection.getresponse()
        if read:response.read();connection.close()
        return connection,response

    def test_two_sessions_share_account_limit_other_account_and_metadata_work(self):
        held=[self.request('a',read=False),self.request('a-other-session',read=False)]
        _,denied=self.request('a');self.assertEqual(denied.status,429);self.assertEqual(denied.getheader('Retry-After'),'60')
        held.append(self.request('b',read=False))
        self.assertTrue(all(response.status==200 for _,response in held))
        self.assertEqual(self.request('a','/catalog.json')[1].status,200)
        self.assertEqual(self.request('a',method='HEAD')[1].status,200)
        self.release.set()
        for connection,response in held:self.assertEqual(response.read(),b'fixture-package');connection.close()
        self.assertEqual(self.request('a')[1].status,200)

    def test_bad_legacy_attempts_do_not_lock_out_verified_tunnel_user(self):
        for _ in range(10):self.assertEqual(self.request('',path='/catalog.json')[1].status,401)
        self.assertEqual(self.request('',path='/catalog.json')[1].status,429)
        self.assertEqual(self.request('a',path='/catalog.json')[1].status,200)


if __name__=='__main__':unittest.main()
