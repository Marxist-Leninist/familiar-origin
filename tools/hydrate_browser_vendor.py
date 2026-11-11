#!/usr/bin/env python3
"""Restore exact official third-party browser assets, then verify every byte."""
from pathlib import Path
import base64,hashlib,io,json,shutil,subprocess,tarfile,tempfile,urllib.request
ROOT=Path(__file__).resolve().parent.parent
VENDOR=ROOT/'browser-site/dist/vendor';VENDOR.mkdir(parents=True,exist_ok=True)
def fetch(url):
 with urllib.request.urlopen(url,timeout=90) as r:return r.read()
with tempfile.TemporaryDirectory(prefix='origin-vendor-') as t:
 temp=Path(t)
 package=fetch('https://registry.npmjs.org/v86/-/v86-0.5.473.tgz')
 assert base64.b64encode(hashlib.sha512(package).digest()).decode()=='mNHB0DXDmfxn0CrlqCmlgMR1W7aKuD+sHmSctLcTROOwpOvbE4qsAhmaE6Vy+riQfcE4PfA2xvW59/ZXsYnHNw=='
 with tarfile.open(fileobj=io.BytesIO(package),mode='r:gz') as archive:
  for name in ['libv86.js','libv86.mjs','v86.wasm','v86-fallback.wasm']:
   (VENDOR/name).write_bytes(archive.extractfile('package/build/'+name).read())
 deb=fetch('https://deb.debian.org/debian/pool/main/s/seabios/seabios_1.16.3-2_all.deb')
 metadata=json.loads((ROOT/'evidence/qemu-packages.json').read_text())
 assert hashlib.sha256(deb).hexdigest()==next(p['sha256'] for p in metadata if p['name']=='seabios')
 (temp/'seabios.deb').write_bytes(deb)
 subprocess.run(['dpkg-deb','-x',str(temp/'seabios.deb'),str(temp/'firmware')],check=True)
 for src,dst in [('bios-256k.bin','seabios.bin'),('vgabios-stdvga.bin','vgabios.bin')]:shutil.copyfile(temp/'firmware/usr/share/seabios'/src,VENDOR/dst)
 for name,url in {
  'v86-source.tar.gz':'https://codeload.github.com/copy/v86/tar.gz/315b2b0',
  'seabios-source.tar.gz':'https://deb.debian.org/debian/pool/main/s/seabios/seabios_1.16.3.orig.tar.gz',
  'seabios-debian.tar.xz':'https://deb.debian.org/debian/pool/main/s/seabios/seabios_1.16.3-2.debian.tar.xz',
 }.items():(VENDOR/name).write_bytes(fetch(url))
manifest=json.loads((ROOT/'browser-site/dist/assets/vendor-manifest.json').read_text())
for relative,expected in manifest.items():
 path=ROOT/'browser-site/dist'/relative
 assert path.stat().st_size==expected['bytes'],relative
 assert hashlib.sha256(path.read_bytes()).hexdigest()==expected['sha256'],relative
print('PASS: all browser vendor files match the recorded SHA-256 manifest.')
