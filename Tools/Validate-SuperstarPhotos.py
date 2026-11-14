"""Check that a real engine smoke publication contains immutable, exact pixel crops."""
from pathlib import Path
import json,argparse
from PIL import Image,ImageChops
p=argparse.ArgumentParser(__doc__)
p.add_argument('publication',type=Path)
a=p.parse_args(); manifest=json.loads(a.publication.read_text(encoding='utf-8-sig'))
assert len(manifest['shots'])==3
assert len({s['beat'] for s in manifest['shots']})==3,'Smoke run must visit three different moments'
assert len({s['station'] for s in manifest['shots']})==3,'Smoke run must cover three stations'
for shot in manifest['shots']:
    original=Image.open(a.publication.parent/shot['original']).convert('RGB')
    frame=Image.open(a.publication.parent/shot['image']).convert('RGB')
    x,y,w,h=[shot[k] for k in ['crop_left','crop_top','crop_width','crop_height']]
    assert 0<=x<=original.width-w and 0<=y<=original.height-h
    assert frame.size==(w,h)
    assert ImageChops.difference(original.crop((x,y,x+w,y+h)),frame).getbbox() is None
    assert sum(maximum-minimum for minimum,maximum in original.getextrema())>200,'Not a blank frame'
assert any(s['crop_left']>0 or s['crop_top']>0 for s in manifest['shots'])
print('PHOTO FILES PASS: 3 different moments and stations; exact original-pixel crops, valid bounds, nonblank PNGs.')
