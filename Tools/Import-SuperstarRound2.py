"""Import the reviewed Round2 return into versioned folders; preserve original scenes and history.
Usage: python Tools/Import-SuperstarRound2.py --zip <return.zip>
Only frame/metadata payloads are read. No delivery scripts or old project snapshots are executed.
"""
from pathlib import Path
import argparse,hashlib,io,json,re,zipfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'Content/SuperstarStory'
VERSION='Round2Actual20261115'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def save(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 ap=argparse.ArgumentParser(__doc__);ap.add_argument('--zip',type=Path,required=True);args=ap.parse_args()
 z=zipfile.ZipFile(args.zip);manifest=json.loads(z.read('DELIVERY.sha256.json'));report=json.loads(z.read('delivery_report.json'))
 def verified(n):
  data=z.read(n);assert manifest.get(n)==sha(data),f'Checksum mismatch: {n}';return data
 ids=report['selected'];assert len(ids)==78 and len(set(ids))==78
 metadata={};anchors={};payload={}
 for id in ids:
  assert re.fullmatch(r'[A-Z][0-9]{2}_[a-z0-9_]+',id),id
  prefix=f'output/animations/{id}/';m=json.loads(verified(prefix+'animation.json'));a=json.loads(verified(prefix+'anchors.json'))
  assert m['id']==id and m['fps']==5 and m['canvas']==[640,640] and m['anchor_pixels']==[320,570]
  assert len(a['frames'])==m['frame_count'] and len(m['frame_mapping'])==m['frame_count']
  frames=[]
  for i,f in enumerate(m['frame_mapping'],1):
   assert f['file']==f'frames/{id}_{i:06d}.png'
   data=verified(prefix+f['file']);im=Image.open(io.BytesIO(data));assert im.mode=='RGBA' and im.size==(640,640)
   assert im.getchannel('A').getextrema()[0]==0 and im.getbbox() is not None
   Image.open(io.BytesIO(data)).verify();frames.append(data)
  for f in a['frames']:
   assert all(0<=v<640 for v in f['head'])
   if f.get('hand') is not None:assert all(0<=v<640 for v in f['hand'])
  metadata[id]=m;anchors[id]=a;payload[id]=frames
 history=DEST/'ImportHistory'/VERSION
 for n in ['clips.json','story.json','head_anchors.json']:
  if not (history/n).exists():save(history/n,read(DEST/n))
 clips=read(history/'clips.json');story=read(history/'story.json');heads=read(history/'head_anchors.json')
 for id in ids:
  m=metadata[id];a=anchors[id];path=f'Animations/{VERSION}/{id}'
  folder=DEST/path;folder.mkdir(parents=True,exist_ok=True)
  for i,data in enumerate(payload[id],1):(folder/f'{id}_{i:06d}.png').write_bytes(data)
  save(folder/'animation.json',m);save(folder/'anchors.json',a)
  clips[id]=dict(frames=m['frame_count'],fps=5,loop=m['loop'],pivot=[.5,.890625],path=path,sha256=[sha(b) for b in payload[id]],screen_facing=m['screen_facing'],prop_mode=a['prop_mode'],source_sha256=m.get('source_sha256'),delivery=VERSION)
  hands=[f.get('hand') for f in a['frames']]
  if all(h is not None for h in hands):clips[id]['hand_anchors']=hands
  heads[id]=[f['head'] for f in a['frames']]
 # A28 is an empty-handed gait; measured visible hand centers add the carried folder grip.
 clips['A28_chenmo_walk']['hand_anchors']=[[282,370],[251,373],[234,372],[266,373],[305,368],[338,356],[325,360]]
 clips['A28_chenmo_walk']['hand_anchor_source']='UE integration: visible near-hand skin centers, source frames unchanged'
 # Reference matching uses actual rendered contact geometry, not fixed screen offsets.
 def cid(prefix):
  matches=[n for n in clips if n.startswith(prefix+'_')];assert len(matches)==1,(prefix,matches);return matches[0]
 def dur(prefix):return clips[cid(prefix)]['frames']/5
 beats=story['beats']
 def actor(n,i,base,seq=(),walk=None,move=None,facing=None):
  a=beats[n-1]['actors'][i]
  for k in ['sequence','delay','hold_frame','walk_clip','move_start','move_end','hand','presentation','motion_offsets','move_facing']:a.pop(k,None)
  a['clip']=cid(base);a['name']=a['name'].split(' · ')[0];a['emotion_placeholder']=False
  if seq:a['sequence']=[dict(at=t,clip=cid(c)) for t,c in seq]
  if move is not None:
   a['x'],a['to'],a['move_start'],a['move_end']=move;a['walk_clip']=cid(walk or base)
  else:a['to']=a['x']
  if facing is not None:a['facing']=facing
  else:a['facing']='left' if a['flip'] != (clips[a['clip']].get('screen_facing')=='left') else 'right'
  return a
 def add(n,name,base,x,facing='right'):
  beats[n-1]['actors'].append(dict(name=name,clip=cid(base),x=x,to=x,flip=False,facing=facing));return len(beats[n-1]['actors'])-1
 def length(n,t):beats[n-1]['duration']=t
 def clearprops(n):beats[n-1]['props']=[]
 def prop(n,image,owner,size,grip=(.5,.76),**kw):
  p=dict(image=image,x=640,z=150,size=size,depth=-45,attach_actor=owner,grip=list(grip));p.update(kw);beats[n-1].setdefault('props',[]).append(p);return p
 static='Static/Partial20261115/'
 def sync(n,left,right,t,center=640):
  # Make grip points share world X/Z at the transfer, preserving foot origins.
  def pose(a,time):
   id=a['clip'];local=max(0,time-a.get('delay',0));sample=local
   for part in a.get('sequence',[]):
    if local>=part['at']:id=part['clip'];sample=local-part['at']
   c=clips[id];f=min(c['frames']-1,int(round(sample*5)));h=c['hand_anchors'][f]
   flip=(a.get('facing','right')=='left') != (c['screen_facing']=='left')
   return id,h,flip
  aa=beats[n-1]['actors'];li,lh,lf=pose(aa[left],t);ri,rh,rf=pose(aa[right],t)
  scale=(570-lh[1])/max(1,570-rh[1]);assert .65<scale<1.4,(n,scale)
  aa[right].setdefault('presentation',{})[ri]={'scale':scale}
  # Same prop may use different handle points. For rotated knife, U offsets are vertical.
  lx=(640-lh[0] if lf else lh[0])-320;rx=((640-rh[0] if rf else rh[0])-320)*scale
  sep=(lx-rx)*395/640*1280/1400
  aa[left]['x']=aa[left]['to']=center-sep/2;aa[right]['x']=aa[right]['to']=center+sep/2
  return scale
 # Act 1
 actor(1,0,'F03',walk='A01',move=(180,490,0,4.25));actor(1,1,'F21',facing='left');actor(1,2,'F17',facing='left')
 length(2,6);actor(2,0,'A02',[(3.2,'F01')],facing='right');a=actor(2,1,'A03',facing='left');a['delay']=.8;actor(2,2,'F17',facing='left')
 actor(3,0,'A04',[(2.2,'F01')],walk='A04',move=(500,560,0,2.2));actor(3,1,'F21',walk='A06',move=(740,170,0,4.6));actor(3,2,'A05',[(3,'F17')],facing='left')
 length(4,10.6);actor(4,0,'A07',[(4.8,'A09')],facing='right');actor(4,1,'A08',[(3.6,'A10'),(8.6,'F20')],facing='left')
 # Arrival followed by the reviewed transfer; hold the recipient pose through the end.
 sync(4,0,1,6.8,640);a=beats[3]['actors'][1];end=a['x'];a.update(x=end+260,to=end,walk_clip=cid('A08'),move_start=0,move_end=3.6)
 clearprops(4);prop(4,static+'P01_umbrella_closed.png',1,210,transfer_actor=0,transfer_at=6.8)
 length(5,9);actor(5,0,'A11',[(4.4,'A12')],walk='A12',move=(500,1050,4.4,8.8));clearprops(5);prop(5,static+'P02_umbrella_open.png',0,300,visible_clips=[cid('A12')],angle=0)
 actor(6,0,'A13');actor(6,1,'A14',facing='left');actor(6,2,'A15',facing='left');clearprops(6);prop(6,'Static/Motion20261115/phone.png',2,62,(.18,.72))
 # Act 2
 actor(7,0,'F03');actor(7,1,'E01');actor(7,2,'E03',walk='E04',move=(1040,780,0,4.25))
 actor(8,0,'R01',[(2.8,'F03')]);clearprops(8)
 actor(9,0,'F01',walk='A01',move=(470,530,0,2.6));actor(9,1,'S04',walk='C02',move=(1040,720,0,2.6))
 actor(10,0,'C06');actor(10,1,'C03',facing='left')
 actor(11,0,'F01');actor(11,1,'A16',[(2.6,'A17')],facing='left')
 actor(12,0,'F01');actor(12,1,'C04',facing='left')
 actor(13,0,'F03');actor(13,1,'C05',facing='left')
 length(14,4.6);actor(14,0,'F03');actor(14,1,'S04',[(2.6,'F09')],facing='left')
 length(15,5);actor(15,0,'F02');actor(15,1,'F10',walk='C02',move=(730,120,2,5))
 length(16,7);actor(16,0,'A19',[(2.8,'F01')]);actor(16,1,'A18',[(2,'A20')],walk='A18',move=(1080,790,0,2),facing='left')
 length(17,4.4);actor(17,0,'C07');a=actor(17,1,'C08',facing='left');a['delay']=1.2
 length(18,4.2);actor(18,0,'A21',[(3,'F03')]);actor(18,1,'F13',[(1.8,'F14')],facing='left')
 length(19,6.4);actor(19,0,'F02');actor(19,1,'C09',[(4.2,'F12')],facing='left')
 actor(20,0,'F03',walk='A01',move=(230,600,0,4.8));actor(20,1,'E01',walk='E02',move=(1040,850,0,4.8));actor(20,2,'E03')
 length(21,6);actor(21,0,'A01',[(2,'A22')],walk='A01',move=(220,660,0,2))
 length(22,11);actor(22,0,'A22',[(3,'F05')]);beats[21]['actors'][0]['x']=beats[21]['actors'][0]['to']=250
 add(22,'李谋','A23',670);actor(22,1,'A23',[(3.8,'A25'),(8.8,'F18')]);add(22,'苏瑶','A24',950,'left');actor(22,2,'A24',[(4.6,'F15')],facing='left')
 length(23,6.6);actor(23,0,'A26',[(4.4,'F06')])
 # Act 3
 length(24,8);actor(24,0,'A27',[(4,'A30')]);actor(24,1,'A28',[(3,'A29')],walk='A28',move=(180,820,0,3),facing='left')
 length(25,7);actor(25,0,'A32',[(4.2,'F04')]);actor(25,1,'A31',[(3.4,'F19')],facing='left')
 length(26,6);actor(26,0,'F04');actor(26,1,'A33',walk='A28',move=(820,200,4.6,6),facing='left')
 length(27,14);actor(27,0,'F04',[(8,'A36'),(11.4,'F05')]);actor(27,1,'A18',[(2,'A34'),(7.2,'A35'),(12,'F15')],walk='A18',move=(220,820,0,2),facing='left')
 length(28,7.4);actor(28,0,'A40',facing='right');actor(28,1,'A39',facing='left');a=actor(28,2,'A37',facing='right');a['delay']=.8
 add(28,'工作人员','A38',470,'left');beats[27]['actors'][3]['delay']=1.6
 sync(28,0,3,2.6,370);sync(28,2,1,3.8,900)
 # Match the delivered two-hand contact layout: separate blade/handle grips, not coincident palms.
 gap=92/.49*395/640*1280/1400
 for i,x in [(0,370-gap/2),(3,370+gap/2)]:beats[27]['actors'][i]['x']=beats[27]['actors'][i]['to']=x
 clearprops(28);prop(28,static+'P04_prop_knife.png',3,100,(.65,.458),transfer_actor=0,transfer_at=2.6,transfer_grip=[.34375,.45703125],angle=-90,mirror_with_holder=False)
 prop(28,static+'P05_white_rose.png',2,130,(.482421875,.751953125),transfer_actor=1,transfer_at=4,mirror_with_holder=False)
 actor(29,0,'A41',facing='right');actor(29,1,'A39',facing='left')['hold_frame']=24
 clearprops(29);prop(29,static+'P04_prop_knife.png',0,100,(.34375,.457),angle=-90,mirror_with_holder=False);prop(29,static+'P05_white_rose.png',1,130,(.482421875,.751953125))
 length(30,5.6);actor(30,0,'A41',facing='right');actor(30,1,'A42',facing='left');clearprops(30)
 prop(30,static+'P04_prop_knife.png',0,100,(.34375,.457),angle=-90,mirror_with_holder=False);prop(30,static+'P05_white_rose.png',1,130,(.482421875,.751953125))
 length(31,11);actor(31,0,'A56',[(5,'A44')],walk='A44',move=(550,180,5,11),facing='right');actor(31,1,'F16',[(2,'A43')],facing='left')
 actor(31,2,'E01',[(5,'A45')],walk='A45',move=(550,180,5,11),facing='left');add(31,'工作人员','A55',650,'left');actor(31,3,'A55',[(5,'A46')],walk='A46',move=(550,180,5,11),facing='left');beats[30]['actors'][3]['delay']=.2
 # Only the pre-escort action uses .2 delay; phase all escort loops together.
 beats[30]['actors'][3]['sequence'][0]['at']=4.8
 sync(31,0,3,1.8,550)
 gap=87/.49*395/640*1280/1400
 for i,x in [(0,550-gap/2),(3,550+gap/2)]:beats[30]['actors'][i]['x']=beats[30]['actors'][i]['to']=x
 for i in [0,2,3]:
  a=beats[30]['actors'][i];a['x']=a['to'] if i!=2 else 550;a['to']=a['x']-370;a['move_start']=5;a['move_end']=11
 # Per-frame escort contacts derived from the actual guide hand and Lin Wan upper arms.
 arm=json.loads(verified('review/A44-upper-arm-targets.json'));guide=json.loads(verified('review/A46-guide-points.json'))
 leader=beats[30]['actors'][0];leadx=leader['x']
 for index,prefix,scale,targets,hands in [(2,'A45',.59/.51,arm['near'],[f['hand'] for f in anchors[cid('A45')]['frames']]),(3,'A46',.62/.51,arm['far'],guide['guiding'])]:
  a=beats[30]['actors'][index];a.setdefault('presentation',{})[cid(prefix)]={'scale':scale}
  offsets=[]
  for target,hand in zip(targets,hands):
   # All escort clips face left in UE; character-pixel X is mirrored.
   dx=((320-target[0])-(320-hand[0])*scale)*395/640*1280/1400
   dz=((570-target[1])-(570-hand[1])*scale)*395/640
   offsets.append([leadx-a['x']+dx,dz])
  a['presentation'][cid(prefix)]['offsets']=offsets
 clearprops(31);prop(31,static+'P04_prop_knife.png',0,100,(.34375,.457),transfer_actor=3,transfer_at=1.8,transfer_grip=[.65,.476],angle=-90,mirror_with_holder=False)
 length(32,8);actor(32,0,'F05',[(2,'A48')],facing='left');actor(32,1,'A47',facing='right')['delay']=3.6
 sync(32,1,0,4.6,650);a=beats[31]['actors'][1];end=a['x'];a.update(x=end-240,to=end,walk_clip=cid('A28'),move_start=0,move_end=2)
 clearprops(32);prop(32,static+'P06_document_folder.png',1,120,(.5,.6),transfer_actor=0,transfer_at=4.6,mirror_with_holder=False)
 # Act 4: distinct speaking/send actions, all existing evidence gates preserved.
 length(33,11.2);actor(33,0,'A49',[(3.4,'F08'),(5.4,'F07'),(7.4,'A50')])
 for n in [34,35,36]:
  length(n,6);actor(n,0,'A50',[(2,'A51')]);prop(n,'Static/Motion20261115/phone.png',0,62,(.18,.72),visible_clips=[cid('A51')])
 actor(34,1,'F19',facing='left');actor(35,1,'F11',walk='A52',move=(1030,760,0,3));actor(35,2,'F19',facing='left')
 actor(36,1,'F11');actor(36,2,'E01',walk='E02',move=(1080,850,0,4));actor(36,3,'E03',walk='E04',move=(1150,1030,0,4))
 length(38,9);actor(38,0,'F02',[(3,'A09')],facing='right');actor(38,1,'A53',[(3,'A54')],facing='left')
 sync(38,0,1,5,700);a=beats[37]['actors'][1];end=a['x'];a.update(x=end+280,to=end,walk_clip=cid('A53'),move_start=0,move_end=3)
 clearprops(38);prop(38,static+'P01_umbrella_closed.png',1,210,transfer_actor=0,transfer_at=5)
 length(39,9);actor(39,0,'A11',[(4.4,'A12')],walk='A12',move=(700,1080,4.4,8.8));actor(39,1,'F11');clearprops(39);prop(39,static+'P02_umbrella_open.png',0,300,visible_clips=[cid('A12')])
 # Disable stale prototype overlays and keep all old bytes for rollback.
 used=set()
 for b in beats:
  for a in b['actors']:
   used.add(a['clip']);used.update(s['clip'] for s in a.get('sequence',[]))
   if a.get('walk_clip'):used.add(a['walk_clip'])
  b['note']='第二轮回传动作已接入；5 FPS逐帧取样，道具按握点附着。'
 for id,c in clips.items():c['retired']=id not in used
 assert set(ids)<=used,sorted(set(ids)-used)
 # Persist reviewed input without old runtime/config copies from the archive.
 # Add transient support geometry only to the matching shots; old maps are untouched.
 def box(n,x,y,z,w,d,h):beats[n-1].setdefault('support_boxes',[]).append(dict(position=[(x-640)*1400/1280,y,z],size=[w,d,h],color=[.30,.21,.14]))
 for n in [1,2,3]:
  box(n,630,0,165.5,450,100,12)
  for x in [440,820]:
   for y in [-35,35]:box(n,x,y,78,10,10,156)
 for n in [24,25,26,27]:
  box(n,605,-16,74,90,75,10)
  for x in [575,635]:
   for y in [-40,10]:box(n,x,y,35,7,7,70)
 record=dict(version=VERSION,source_zip_sha256=sha(args.zip.read_bytes()),imported_ids=ids,frame_count=sum(len(v) for v in payload.values()),delivery_report=report,level_seconds=[sum(b['duration'] for b in beats if b['chapter']==i) for i in range(1,5)],status='integrated_pending_engine_validation')
 save(DEST/'round2-integration.json',record);save(DEST/'clips.json',clips);save(DEST/'story.json',story);save(DEST/'head_anchors.json',heads)
 for group in range(1,11):
  n=f'output/sync/G{group:02d}/interaction.json';save(history/f'G{group:02d}-source-review.json',json.loads(verified(n)))
 print(f'Imported {len(ids)} clips / {record["frame_count"]} frames; level seconds {record["level_seconds"]}')
if __name__=='__main__':main()
