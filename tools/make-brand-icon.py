"""Package the approved launcher artwork as desktop icons; never redraw the logo.

Regeneration uses Pillow 11.2.1. Binaries are committed, so release runners do not
need Pillow. The native PNG is an exact copy; ICO/ICNS only supply OS icon sizes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import PIL
from PIL import Image

root = Path(__file__).resolve().parents[1]
brand = root / 'assets/brand'
manifest = json.loads((root / 'tools/launcher-favicon-v1.json').read_text(encoding='utf-8-sig'))
source = root / 'website/assets/favicons/v1/launcher/favicon-512.png'
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
assert source_hash == manifest['files']['favicon-512.png']['sha256'], 'Approved artwork changed'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true', help='Verify committed desktop icons without writing')
args = parser.parse_args()
names = ('TeamHJD-AppIcon.png', 'TeamHJD.ico', 'TeamHJD.icns')
proof_path = brand / 'app-icon-provenance.json'
if not args.check:
    assert PIL.__version__ == '11.2.1', 'Use Pillow 11.2.1 for reproducible regeneration'
    shutil.copyfile(source, brand / names[0])
    image = Image.open(source).convert('RGBA')
    assert image.size == (512, 512), 'Preserve the approved square artwork'
    image.save(brand / names[1], format='ICO', sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
    image.save(brand / names[2], format='ICNS')
    proof = {'sourceRepository': manifest['sourceRepository'], 'sourceCommit': manifest['sourceCommit'],
             'variant': 'launcher', 'source': str(source.relative_to(root)).replace('\\', '/'),
             'sourceSha256': source_hash, 'generator': 'Pillow 11.2.1; OS icon sizing only',
             'files': {name: {'bytes': (brand/name).stat().st_size,
                              'sha256': hashlib.sha256((brand/name).read_bytes()).hexdigest()} for name in names}}
    proof_path.write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
proof = json.loads(proof_path.read_text(encoding='utf-8-sig'))
assert proof['sourceSha256'] == source_hash and proof['sourceCommit'] == manifest['sourceCommit']
for name in names:
    assert (brand/name).stat().st_size == proof['files'][name]['bytes']
    assert hashlib.sha256((brand/name).read_bytes()).hexdigest() == proof['files'][name]['sha256']
assert (brand/names[0]).read_bytes() == source.read_bytes()
image = Image.open(source).convert('RGBA')
with Image.open(brand/'TeamHJD.ico') as ico:
    assert (256, 256) in ico.ico.sizes()
with Image.open(brand/'TeamHJD.icns') as icns:
    assert icns.icns.getimage((512, 512, 1)).convert('RGBA').tobytes() == image.tobytes()
print('PASS approved launcher artwork, ICO sizes, ICNS native pixels and provenance')
