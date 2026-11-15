"""Import the reviewed 20261115 partial delivery into versioned, additive paths.
Input is extracted data, never executable code. Original frame folders remain intact.
"""
from pathlib import Path
import argparse,json,hashlib,shutil
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1];DEST=ROOT/'Content/SuperstarStory'
STAMP='Partial20261115'
IDS=['A16_xiaobei_rise_empty','A17_xiaobei_crouch_empty','A49_linwan_deep_breath','C05_xiaobei_rise_papers','C06_linwan_push_palm','C07_linwan_stamp','C08_suyao_fall_kneel','C09_suyao_rise']
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def save(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def finalize_notes(story):
    notes={1:'角色母版已接入；走路、敲桌与互动动作仍待补。',2:'演员与李谋母版已接入；敲桌、后退撑桌动作待补。',3:'角色身份已替换；鼓掌、退场与回头表演待补。',
           4:'小周母版与收伞图已接入；跑近、夺伞接触动作待补。',5:'雨伞图已接入；撑伞手势、行走与雨效待补。',6:'小夏/男人母版及手机图已接入；拍照、拥抱表演待补。',
           7:'名单证据图已上墙；工作人员继续使用原素材。',24:'陈默母版已接入；走近、敲门与林晚坐姿待补。',25:'陈默母版已接入；安慰、镜前对话和坐姿待补。',26:'陈默母版已接入；离开与回望目前以位移占位。',
           28:'玫瑰和道具刀已摆放；交接与手部接触未完成。',29:'道具已接入；握刀逼近与对白动作仍待补。',32:'陈默母版、文件夹已接入；递交接触动作待补。',
           33:'深呼吸和手机支架已接入；直播发言动作待补。',34:'陈默母版和直播道具已接入；第一张证据照片尚未交付。',35:'陈默母版和直播道具已接入；第二张证据照片尚未交付。',36:'名单证据图已接入；可选择并发送。',
           38:'收伞图已接入；小北走近与交伞动作待补。',39:'开伞图已接入；接触、开伞和持伞走路动作待补。'}
    for n,note in notes.items():story['beats'][n-1]['note']=note

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('output',type=Path);a=parser.parse_args();src=a.output.resolve()
    assert not (DEST/'Animations'/STAMP).exists(),'Do not overwrite a previously imported version'
    manifest=read(src/'MANIFEST.sha256.json')
    for rel,sha in manifest.items():
        p=(src/rel).resolve();assert p.is_relative_to(src) and digest(p)==sha,rel
    clips=read(DEST/'clips.json');story=read(DEST/'story.json');old=read(DEST/'clips.json')
    imported=[]
    for id in IDS:
        meta=read(src/'animations'/id/'animation.json');frames=sorted((src/'animations'/id/'frames').glob('*.png'))
        assert meta['fps']==5 and not meta['loop'] and len(frames)==meta['frame_count']
        assert digest(src/meta['source'])==meta['source_sha256']
        for p in frames:
            with Image.open(p) as im:assert im.mode=='RGBA' and im.size==(640,640)
        path=f'Animations/{STAMP}/{id}';target=DEST/path;target.mkdir(parents=True)
        for p in frames:shutil.copy2(p,target/p.name)
        shutil.copy2(src/'animations'/id/'animation.json',target/'delivery-animation.json')
        clips[id]=dict(frames=len(frames),fps=5,loop=False,pivot=[.5,.890625],path=path,sha256=[digest(p) for p in frames],screen_facing=meta['screen_facing'],source_delivery=STAMP)
        imported.append(id)
    # Separate single-frame character placeholders, explicitly not generated idle animations.
    master_roles=['actress','chenmo','limou','man','xiaoxia','xiaozhou']
    for role in master_roles:
        id='M_'+role;path=f'Actors/{STAMP}/{id}';target=DEST/path;target.mkdir(parents=True)
        master=src/'masters'/f'REF_{role}.png'
        rgb=np.asarray(Image.open(master).convert('RGB')).astype(np.float32)
        excess=rgb[:,:,1]-np.maximum(rgb[:,:,0],rgb[:,:,2])
        alpha=np.clip((120-excess)/100,0,1)
        edge=alpha<.999
        rgb[:,:,1]=np.where(edge,np.minimum(rgb[:,:,1],np.maximum(rgb[:,:,0],rgb[:,:,2])+8),rgb[:,:,1])
        rgba=np.dstack([np.clip(rgb,0,255),np.rint(alpha*255)]).astype(np.uint8);rgba[alpha<=0,:3]=0
        Image.fromarray(rgba).save(target/f'{id}_000001.png')
        p=target/f'{id}_000001.png'
        clips[id]=dict(frames=1,fps=5,loop=False,pivot=[.5,.890625],path=path,sha256=[digest(p)],kind='static_master_placeholder',screen_facing='right',design_status='production_default_not_user_approved',source_sha256=digest(master))
    static_path=DEST/'Static'/STAMP;static_path.mkdir(parents=True)
    for p in (src/'static').iterdir():
        if p.suffix in ['.png','.svg','.json']:shutil.copy2(p,static_path/p.name)
    # Replace only the explicitly named stand-ins, retaining all old extra-role assets.
    for b in story['beats']:
        for actor in b['actors']:
            name=actor['name'];role=None
            for prefix,r in [('李谋','limou'),('陈默','chenmo'),('小周','xiaozhou'),('女主演员','actress'),('小夏','xiaoxia'),('男人','man')]:
                if name.startswith(prefix):role=r;break
            if role:
                actor['clip']='M_'+role;actor['name']=name.split(' · ')[0]+' · 母版占位'
                actor['flip']=actor['to']<actor['x'] or (actor['x']>=650 and role in ['limou','actress','chenmo','man'])
                b['note']+=' 新角色母版已替换路人；专用动作仍待补。'
    beats=story['beats']
    # Keep the 39 story anchors: the empty-handed rise/crouch is a deterministic chain in beat 11.
    actor=beats[10]['actors'][1];actor['clip']='A16_xiaobei_rise_empty';actor['sequence']=[dict(at=2.6,clip='A17_xiaobei_crouch_empty')]
    beats[10]['duration']=5.8;beats[10]['note']='已接入空手起身→再蹲下；地面三张纸已在动画中，不额外叠加。'
    beats[32]['actors'][0]['clip']='A49_linwan_deep_breath';beats[32]['note']='已接入直播前深呼吸；后续讲话、发送手势待补。'
    beats[9]['duration']=max(4.4,old['C03_xiaobei_fall_scatter']['frames']/5)
    beats[12]['note']='抱资料起身已更新抠像及站立基线；后续持纸循环仍用旧版。'
    # New Su Yao source faces left already. Never mirror it a second time.
    for n in [16,18,29]:beats[n]['actors'][1]['flip']=False
    beats[16]['actors'][1]['delay']=1.0
    beats[16]['note']='跺脚压缩悬停；失衡起点延后1秒，与落脚对齐。新版苏瑶跌倒表情已接入。'
    for n in [17,30]:
        beats[n]['actors'][1]['clip']='C08_suyao_fall_kneel';beats[n]['actors'][1]['hold_frame']=13;beats[n]['actors'][1]['flip']=False
    beats[17]['note']='保持新版跌倒的实际末帧接续起身；这是停帧占位，不是已验收的跪姿循环。'
    beats[18]['duration']=4.2;beats[18]['note']='起身已接续新版跪姿；恢复甜笑的表情仍待补，当前保持惊慌。'
    beats[29]['note']='仍用第二幕失衡片占位；第三幕假摔、捂肩与指认动作尚未交付。'
    beats[30]['note']='保持假摔占位的末帧；捂肩指认、三人押送与收刀互动仍待补。'
    # Avoid showing the known wrong whole-photo tearing action while R01 is still rejected.
    beats[7]['actors'][0]['clip']='S01_linwan_idle';beats[7]['note']='完整照片已上墙；扯标签动画R01尚未交付，不再播放错误的撕整张照片。'
    clips['C01_linwan_tear_photo']['retired']=True;clips['S05_suyao_kneel_idle']['retired']=True
    # Keep both required extras available in the new story.
    beats[19]['actors'].append(dict(name='工作人员',clip='E03_extra_m_idle',x=920,to=920,flip=False))
    def prop(n,id,x,z,size,actor=None,dx=0,depth=-60):
        item=dict(image=f'Static/{STAMP}/{id}.png',x=x,z=z,size=size,depth=depth)
        if actor is not None:item.update(actor=actor,offset_x=dx)
        beats[n-1].setdefault('props',[]).append(item)
    prop(4,'P01_umbrella_closed',450,140,160,actor=1,dx=85)
    prop(5,'P02_umbrella_open',700,290,260,actor=0,dx=85)
    prop(6,'P03_phone',1030,185,80,actor=2,dx=-70)
    prop(7,'EV03_roster',300,275,170,depth=270)
    prop(8,'P07_photo_intact',390,270,160,depth=270)
    prop(28,'P04_prop_knife',580,160,95,actor=0,dx=65)
    prop(28,'P05_white_rose',820,175,130,actor=1,dx=-65)
    prop(29,'P04_prop_knife',550,160,95,actor=0,dx=65)
    prop(29,'P05_white_rose',790,175,130,actor=1,dx=-65)
    prop(30,'P04_prop_knife',550,160,95,actor=0,dx=65)
    prop(32,'P06_document_folder',470,130,125,actor=1,dx=70)
    for n in [33,34,35,36]:
        prop(n,'P10_phone_stand',1000,125,150);prop(n,'P03_phone',1000,170,100)
    prop(36,'EV03_roster',260,280,170,depth=270)
    beats[35]['evidence_image']=f'Static/{STAMP}/EV03_roster.png'
    prop(38,'P01_umbrella_closed',480,140,160,actor=1,dx=80)
    prop(39,'P02_umbrella_open',800,290,260,actor=0,dx=85)
    for b in beats:
        if any('P0' in p['image'] or 'P10' in p['image'] for p in b.get('props',[])):
            b['note']+=' 道具已摆放，手部接触/交接动画未完成。'
    # The two reserved props are not duplicated over sheets/labels baked into existing images.
    inventory=dict(delivery=STAMP,animation_ids=imported,masters=['M_'+r for r in master_roles],static_files=[p.name for p in static_path.glob('*.png')],reserved=['P08_old_label','P09_three_papers'],remaining=read(src/'work_status.json')['remaining'],source_delivery_sha256='042237c72508f9682f552a0eb538b77c2edb4192ae45c93c611a1ae65e8f6f1b',status='partial_integration_not_full_acceptance')
    finalize_notes(story)
    save(DEST/'clips.json',clips);save(DEST/'story.json',story);save(DEST/'partial-integration.json',inventory)
    save(DEST/'ImportHistory'/f'{STAMP}-previous-clips.json',old)
    print('IMPORTED: 8 animation clips, 6 keyed static character placeholders, 11 static images; original frame folders preserved.')

if __name__=='__main__':main()
