"""Apply reviewed motion and prop staging to the independent 39-beat story."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]/'Content/SuperstarStory'
def load(n):return json.loads((root/n).read_text(encoding='utf-8'))
def save(n,v):(root/n).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
s=load('story.json');c=load('clips.json');b=s['beats']
roles={'林晚':'linwan','小北':'xiaobei','苏瑶':'suyao','陈默':'chenmo','小周':'xiaozhou','女主演员':'actress'}
for beat in b:
    for a in beat['actors']:
        if a['x']==a['to']:continue
        for prefix,role in roles.items():
            if a['name'].startswith(prefix) and a['clip']!='C02_xiaobei_jog_papers':a['walk_clip']='W_'+role
        old=a['clip']
        if old in ['E02_extra_f_walk','E04_extra_m_walk','C02_xiaobei_jog_papers']:
            a['walk_clip']=old;a['clip']={'E02_extra_f_walk':'E01_extra_f_idle','E04_extra_m_walk':'E03_extra_m_idle','C02_xiaobei_jog_papers':'S04_xiaobei_hold_papers'}[old]
        a['move_start']=0;a['move_end']=round(beat['duration']*.85,2)
        # Leave movement direction to the evaluated source-facing metadata.
for id in ['E01_extra_f_idle','E02_extra_f_walk','E03_extra_m_idle','E04_extra_m_walk','C02_xiaobei_jog_papers','S04_xiaobei_hold_papers']:
    c[id]['screen_facing']='right'
for n in [4,5,28,29,30,39]:b[n-1]['actors'][0]['clip']='H_linwan'
b[5]['actors'][2].update(clip='H_linwan_phone',flip=True)
for n in [28,29]:b[n-1]['actors'][1].update(clip='H_suyao',flip=False)
b[37]['actors'][1]['clip']='H_xiaobei'
# Approach, then hand the umbrella over, then depart. No simultaneous sliding apart.
b[3]['actors'][0].update(x=540,to=820,move_start=2.7,move_end=4.7)
b[3]['actors'][1].update(x=180,to=500,move_start=0,move_end=2.6)
b[37]['actors'][1].update(x=200,to=640,move_start=0,move_end=3.0)
b[37]['actors'][0]['clip']='H_linwan'
grips={'P01':[.5,.76],'P02':[.5,.76],'P04':[234/512,336/512],'P05':[247/512,385/512],'P06':[95/512,270/512]}
for i,beat in enumerate(b,1):
    props=[]
    for p in beat.get('props',[]):
        image=p['image'];key=Path(image).name[:3]
        if image.endswith('/tripod.png'):continue
        if key in ['P03','P10'] and i in [33,34,35,36]:continue
        if 'actor' in p:
            owner=p.pop('actor');p.pop('offset_x',None);p['attach_actor']=owner
            p['grip']=grips.get(key,[.18,.72]);p['depth']=-45
            if key=='P03':p.update(image='Static/Motion20261115/phone.png',size=38)
            if key=='P02':p['size']=300
            if key=='P01':p['size']=210
            if key=='P06':beat['actors'][owner]['hand']=[370,360]
        if key=='P02':p['angle']=12
        if key=='P04':p['angle']=45
        if key=='P05':p.update(size=90,angle=-12)
        props.append(p)
    if i in [33,34,35,36]:
        props.append(dict(image='Static/Motion20261115/tripod.png',x=beat['actors'][0]['x']+130,z=165,size=330,depth=-65))
    if props:beat['props']=props
    if i in [4,38]:
        p=beat['props'][0];p.update(transfer_actor=0,transfer_at=2.6 if i==4 else 3.1)
    if any(a.get('walk_clip') for a in beat['actors']):
        beat['note']=beat['note'].replace('走路、','').replace('行走与','').replace('跑近、','')
    if any('attach_actor' in p for p in props):
        beat['note']=beat['note'].replace('道具已摆放，手部接触/交接动画未完成。','道具按逐帧手部握点跟随。')
        beat['note']=beat['note'].replace('撑伞手势、','').replace('拍照、','')
for i,key in [(34,'ev01'),(35,'ev02')]:
    b[i-1]['evidence_image']=f'Static/Motion20261115/{key}.png'
    b[i-1]['note']='剧本证据图已补齐；直播手机与支架为完整组件，靠近林晚摆放。'
b[3]['note']='小周走近递伞，握点切换给林晚后离开。'
b[4]['note']='林晚以5 FPS迈步，伞柄逐帧贴合手部；雨效仍待补。'
b[5]['note']='林晚转向拍摄对象，手机贴合抬起的手；拥抱表演待补。'
b[37]['note']='小北走近递伞，停步后伞柄转到林晚手中。'
b[38]['note']='林晚握伞向右离开，小北留在原地。'
save('story.json',s);save('clips.json',c)
print('Configured all moving actors, attached props, umbrella ownership, nearby livestream rig, and three evidence cards.')
