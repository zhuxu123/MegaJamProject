"""Validate Round2 runtime reachability, complete actions and isolated level/story contracts."""
from pathlib import Path
import json,math
ROOT=Path(__file__).resolve().parents[1]/'Content/SuperstarStory'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
c=read(ROOT/'clips.json');s=read(ROOT/'story.json');r=read(ROOT/'round2-integration.json')
old=read(ROOT/'ImportHistory/Round2Actual20261115/story.json')
assert len(r['imported_ids'])==78 and r['frame_count']==1168
seen={};phases={}
def sample(a,t):
 local=max(0,t-a.get('delay',0));id=a['clip']
 for q in a.get('sequence',[]):
  if max(0,t-a.get('delay',0))>=q['at']:id=q['clip'];local=t-a.get('delay',0)-q['at']
 if a.get('walk_clip') and a['move_start']<=t<a['move_end']:id=a['walk_clip'];local=t-a['move_start']
 count=c[id]['frames'];f=math.floor(local*5+1e-5);f=f%count if c[id]['loop'] else min(count-1,f)
 if 'hold_frame' in a and not(a.get('walk_clip') and a['move_start']<=t<a['move_end']):f=a['hold_frame']
 return id,f
for n,b in enumerate(s['beats']):
 o=old['beats'][n]
 for field in ['title','description','scene','chapter','evidence']:assert b[field]==o[field],(n,field)
 for a in b['actors']:
  assert not a.get('emotion_placeholder'),(n,a['name'])
  if a.get('walk_clip'):
   assert 0<=a['move_start']<a['move_end']<=b['duration']
   if not c[a['walk_clip']]['loop']:assert a['move_end']-a['move_start']<=c[a['walk_clip']]['frames']/5+.001
  for id,p in a.get('presentation',{}).items():
   assert .65<p.get('scale',1)<1.4
   if 'offsets' in p:assert len(p['offsets'])==c[id]['frames']
  for k in range(math.ceil(b['duration']*50)):
   t=k/50;id,f=sample(a,t);seen.setdefault(id,set()).add(f)
 for p in b.get('props',[]):
  if 'attach_actor' in p:
   for k in range(math.ceil(b['duration']*5)):
    t=k/5;holder=p.get('transfer_actor') if t>=p.get('transfer_at',1e6) else p['attach_actor'];id,f=sample(b['actors'][holder],t)
    if p.get('visible_clips') and id not in p['visible_clips']:continue
    if c[id].get('prop_mode')=='baked':continue
    assert len(c[id].get('hand_anchors',[]))==c[id]['frames'],(n+1,id,'missing hand')
for id in r['imported_ids']:
 assert seen.get(id)==set(range(c[id]['frames'])),(id,'unplayed frames',set(range(c[id]['frames']))-seen.get(id,set()))
 assert not c[id].get('retired')
assert s['beats'][16]['actors'][1]['delay']==1.2
# Single umbrella instance per handover, and a single knife through confiscation/escort.
for n,image in [(4,'P01_umbrella_closed'),(38,'P01_umbrella_closed'),(31,'P04_prop_knife')]:
 assert sum(image in p['image'] for p in s['beats'][n-1]['props'])==1
assert [sum(b['duration'] for b in s['beats'] if b['chapter']==i) for i in range(1,5)]==r['level_seconds']
print('PASS: all 78 returned clips / 1168 frames reachable; every action completes; captions, 39 beats, scenes, evidence and chapter identity preserved; attached props have per-frame grips.')
