"""Validate four chapter partitions and actual per-level photo publications from a new smoke log."""
from pathlib import Path
import argparse,json,re
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
p=argparse.ArgumentParser(__doc__);p.add_argument('--log',type=Path);a=p.parse_args()
levels=read(ROOT/'Content/SuperstarStory/levels.json')['levels'];beats=read(ROOT/'Content/SuperstarStory/story.json')['beats']
assert len(levels)==4
covered=[];interludes=[]
for i,l in enumerate(levels,1):
 assert l['id']==i and len(l['headlines'])==3 and len(set(l['headlines']))==3
 indices=list(range(l['first_beat'],l['last_beat']+1));covered+=indices;interludes+=l['interludes']
 assert all(beats[n-1]['chapter']==i for n in indices)
 assert all(n in indices and '间幕' in beats[n-1]['title'] for n in l['interludes'])
 print(f'Level {i}: beats {indices[0]}-{indices[-1]}, {sum(beats[n-1]["duration"] for n in indices):.1f}s, interludes={l["interludes"]}')
assert covered==list(range(1,len(beats)+1)) and interludes==[6,32]
print('CONFIG PASS: 39 beats partitioned once into 4 independent levels, both interludes retained.')
if a.log:
 text=a.log.read_text(encoding='utf-8-sig');assert 'SuperstarLevels smoke PASS' in text
 dirs=re.findall(r'SuperstarPhoto: publication saved [^\r\n]*?Saved/SuperstarPhotos/([^\r\n]+)',text)
 assert len(dirs)==4,dirs
 sessions=set();seen=[]
 for d in dirs:
  folder=ROOT/'Saved/SuperstarPhotos'/d;data=read(folder/'publication.json');level=levels[data['level']-1];seen.append(data['level']);sessions.add(folder.parent)
  assert data['level_title']==level['title'] and data['headline'] in level['headlines']
  assert data['first_beat']==level['first_beat'] and data['last_beat']==level['last_beat']
  assert len(data['shots'])==3 and len({s['beat'] for s in data['shots']})==3
  for s in data['shots']:
   assert level['first_beat']<=s['beat']<=level['last_beat']
   with Image.open(folder/s['original']) as orig,Image.open(folder/s['image']) as frame:
    x,y,w,h=[s[k] for k in ['crop_left','crop_top','crop_width','crop_height']]
    assert frame.size==(w,h) and 0<=x<=orig.width-w and 0<=y<=orig.height-h
    assert ImageChops.difference(orig.convert('RGB').crop((x,y,x+w,y+h)),frame.convert('RGB')).getbbox() is None
  expected_evidence=[n for n in range(level['first_beat'],level['last_beat']+1) if beats[n-1].get('evidence')]
  assert data['published_evidence_beats']==expected_evidence
  if level['id'] in [1,3]:assert any(s['beat'] in level['interludes'] for s in data['shots'])
 assert seen==[1,2,3,4] and len(sessions)==4
 print('PUBLICATION PASS: 4 unique sessions/newspapers, 12 exact photo crops, no cross-level photos; both interludes photographed.')
