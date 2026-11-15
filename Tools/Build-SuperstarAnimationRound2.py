"""Build a portable second animation handoff from tracked inputs; never reads Desktop files."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'Superstar-Animation-Round2-20261115'
BASE = ROOT / 'Deliverables/Superstar-MissingAssets-20261115.zip'
CONTENT = ROOT / 'Content/SuperstarStory'
DONE = {'A16_xiaobei_rise_empty','A17_xiaobei_crouch_empty','A49_linwan_deep_breath',
        'C05_xiaobei_rise_papers','C06_linwan_push_palm','C07_linwan_stamp','C08_suyao_fall_kneel','C09_suyao_rise'}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,v):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(v if isinstance(v,str) else json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def copyfile(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)


def build(out,old):
    clips=read(CONTENT/'clips.json'); chars=read(old/'config/characters.json')
    for c in chars:
        if 'M_'+c['id'] in clips:
            files=sorted((CONTENT/clips['M_'+c['id']]['path']).glob('*.png'))
            target='references/masters/'+c['id']+'.png';copyfile(files[0],out/target)
            c.update(reference=target,identity_source=target,design_status='integrated_production_default_keep_consistent')
        else:copyfile(old/c['reference'],out/c['reference'])
        c.pop('folder',None) # original folders contain swapped role names
    write(out/'config/characters.json',chars)
    for source in ['requirements.txt','workflows/h3_i2v_640_api.json','workflows/h3_i2v_original_api.json',
                   'sources/H3-sprite-workflow.zip','sources/绿幕动画帧导出工作流.md','sources/剧本全.docx',
                   'config/models.json','config/confirmed_and_defaults.json',
                   'tools/comfy_jobs.py','tools/export_animation.py','tools/extract_green_screen.py','tools/keyframe_from_video.py']:
        copyfile(old/source,out/source)
    copyfile(old/'docs/legacy_02_环境与工作流.md',out/'docs/环境与工作流快照.md')
    # Copy complete current frames and props, not an obsolete 18-clip registry or source archives.
    for folder in ['Animations','Actors','Static','References']:
        shutil.copytree(CONTENT/folder,out/'current'/folder)
    for name in ['clips.json','head_anchors.json','story.json','partial-integration.json','motion-integration.json']:
        copyfile(CONTENT/name,out/'current'/name)
    for name in ['SuperstarStory.cpp','SuperstarStage.cpp','SuperstarTransport.cpp']:
        copyfile(ROOT/'Source/MegaJamProject'/name,out/'integration/runtime_snapshot'/name)
    for name in ['Partial-20261115.md','Motion-20261115.md','Transport-20261115.md']:
        copyfile(ROOT/'Docs/SuperstarStory'/name,out/'integration/history'/name)
    jobs=[j for j in read(old/'config/jobs.json') if j['id'] not in DONE]
    repair=copy.deepcopy(jobs[0]);repair.update(id='C03_xiaobei_fall_scatter',character='xiaobei',name='跌倒散落资料：修正慌乱表情与终点衔接',beats=[10],kind='repair',loop=False,
        start_pose='Match the final running pose of current C02, carrying papers, facing screen right.',
        action='Lose balance once and fall into a low kneeling pose as the papers scatter. Eyes widen, brows lift and the mouth opens in a short startled gasp BEFORE the fall; no smile. End at the compatible low pose of current A16 frame zero and C04 frame zero.',
        expression='startled panic, then anxious embarrassment',events=['action_onset','loss_of_balance','ground_contact','settled_end'],
        dependencies=['baseline:C02_xiaobei_jog_papers'],dependency=None,seed=202611159003)
    jobs.append(repair)
    bychar={c['id']:c for c in chars}
    held={9:'umbrella',10:'umbrella',11:'umbrella',12:'umbrella',15:'phone',37:'rose',38:'knife',39:'rose',40:'knife',41:'knife',42:'rose',47:'file',48:'file',51:'phone',53:'umbrella',54:'umbrella',55:'knife',56:'knife'}
    order=[]
    for j in jobs:
        id=j['id']; c=bychar[j['character']]
        j['dependencies']=[('baseline:M_'+d[4:]) if d.startswith('REF_') else d for d in j.get('dependencies',[])]
        if id.startswith(('F13_','F14_')):
            j['dependencies']=['baseline:C08_suyao_fall_kneel']
            j['start_pose']='Use exactly the LAST frame of current C08_suyao_fall_kneel: low kneeling silhouette, facing screen left. Preserve its low head height, body scale, dress and foot registration. Never use retired S05.'
        if id.startswith(('F09_','F10_')):
            j['dependencies']=['S04_xiaobei_hold_papers']
            j['start_pose']='Use the approved S04 standing pose holding papers against the torso, including the same paper silhouette. Expression is fully established at frame zero.'
        if id.startswith('F12_'):
            j['dependencies']=['A20_suyao_adjust_dress']
            j['start_pose']='Use the final standing pose of A20, same screen-left facing as current C09, with the sweet smile already established.'
        if id.startswith('A20_'):j['start_pose']='Match the exact last frame of current C09_suyao_rise, standing and facing screen left.'
        if id.startswith('A53_'):j['start_pose']='Standing facing screen right, with one empty hand curled at the closed umbrella handle contact point at waist height.'
        if id.startswith('A11_'):
            j['layer_requirement']='Deliver the opening umbrella as part of this baked clip, plus a clean final open-umbrella reference and final hand anchor. Runtime must hide separate umbrella for this clip and restore it for A12. Do not leave the closed umbrella static while the hand opens nothing.'
        if id.startswith('R01_'):
            j['layer_requirement']='Photo and peeling label are baked into this action. Deliver an intact photo/removed-label endpoint reference; runtime must hide duplicate photo props during this clip.'
        num=int(id[1:3]) if id.startswith('A') else None
        prop=held.get(num,'papers' if id.startswith(('S04_','F09_','F10_','C03_')) else 'photo_label' if id.startswith('R01_') else None)
        baked=id.startswith(('A11_','R01_','S04_','F09_','F10_','C03_'))
        mode='baked' if baked else 'attached' if prop else 'none'
        j.update(status='pending',required=True,export_fps=5,canvas=[640,640],anchor=[320,570],
            reference=c['reference'],first_frame=f'inputs/{id}.png',prompt=f'prompts/video/{id}.txt',first_frame_prompt=f'prompts/first_frame/{id}.txt',
            prop_contract={'mode':mode,'prop':prop,'hand_coordinates':'unmirrored 640x640 source pixels; one entry per frame'},
            screen_facing='left' if id.startswith(('F13_','F14_','F12_','A20_','A39_','A42_','A43_','F16_')) else 'right')
        j['dependency']=j['dependencies'][0] if len(j['dependencies'])==1 else None
        j['priority']='P0' if id.startswith(('A01_','A06_','A08_','A09_','A10_','A11_','A12_','A15_','A18_','A28_','A52_','A53_','A54_','E04_','C03_','S04_','F09_','F13_','F14_')) else 'P1'
        identity=c['identity']
        if j['character']=='extra_f' and id.startswith(('A37_','A45_')):
            identity=identity.replace('Carry the same dark clipboard in the left hand and pen in the right hand.','For this interaction leave the hands free of clipboard and pen.')
        style='Match the supplied current character reference exactly; the reference image takes precedence over approximate identity prose. Clean hand-drawn 2D cartoon, consistent outlines and colors, adult proportions. One character only, fixed full-body camera, uniform pure green #00FF00, no ground shadow, no text or icons. 640x640 canvas, standing anchor (320,570), normal standing height approximately 478 pixels. Maintain scale and canvas registration during crouching, kneeling and gestures. Do not recenter frames. '
        pnote=('Final character frames contain NO '+str(prop)+'. Perform the specified grip and contact using an empty, clearly curled hand; the existing separate prop is composited at the measured per-frame hand anchor. The action description specifies intended composite interaction, not an extra baked object. Keep other fixed costume items. ' if mode=='attached' else 'Keep the '+str(prop)+' baked into this clip; preserve its identity and hand contact. No duplicate prop. ' if mode=='baked' else '')
        temporal=('Perform a continuous subtle loop. Frame zero is already on the loop. Return naturally to the same phase, pose and velocity after a complete cycle; do not stop or insert a hold at the seam. ' if j['loop'] else 'Perform once, begin at 0.00 seconds and settle by approximately 4.2 seconds, then hold the final pose. No replay or reversal. ')
        body=style+identity+' Opening: '+j['start_pose']+' Source facing: screen '+j['screen_facing']+'. From 0.00 to 5.00 seconds: '+j['action']+' Facial acting: '+j['expression']+'. '+pnote+temporal+'No reference-card-to-action morph, costume transition, camera pan or introductory idle. '+j.get('layer_requirement','')
        prompt='For the target video, at 0.00 seconds into the target video, <Picture 1> (from [Shot 1]) is fully referenced.\n\nintegrated_multimodal_description: [Shot 1] '+body+'\n\noverall_soundscape: Quiet incidental cloth movement only; no intelligible dialogue. Deliver animation frames without audio.\n\nnon_diegetic_music: None.\n'
        write(out/j['prompt'],prompt)
        write(out/j['first_frame_prompt'],style+'Identity: '+identity+'\nAction-ready pose: '+j['start_pose']+'\nFacial acting: '+j['expression']+'\nUpcoming action (prepare its starting pose only, never multiple phases): '+j['action']+'\n'+pnote+'\nUse the current dependency endpoint when specified. Single action-ready full-body image, never a contact sheet. See config/jobs.json and docs/交付契约.md.\n')
        write(out/f'tasks/{id}.md',f'# {id} · {j["name"]}\n\n优先级：{j["priority"]}；剧情段：{j["beats"]}；循环：{j["loop"]}；朝向：{j["screen_facing"]}。\n\n身份参考：`{j["reference"]}`\n\n依赖：{j["dependencies"] or "无"}。baseline 从 current/clips.json 的 path 定位；REF 已由本批角色母版替代。\n\n首帧提示词：`{j["first_frame_prompt"]}`；视频提示词：`{j["prompt"]}`。\n\n道具：{j["prop_contract"]}。{j.get("layer_requirement","")}\n\n起点：{j["start_pose"]}\n\n动作：{j["action"]}\n\n表情：{j["expression"]}\n\n需要记录的实际事件：{j["events"]}。循环的 cycle_return 标注截断前最后一个有效相位样本；另在 cut.loop_end_seconds 记录被排除的同相位周期终点。\n\n按 docs/交付契约.md 输出，不把当前临时走路或单帧待机算作本任务交付。\n')
    ids={j['id'] for j in jobs};todo=set(ids)
    while todo:
        ready=sorted((id for id in todo if all(d.startswith('baseline:') or d not in todo for d in next(j for j in jobs if j['id']==id)['dependencies'])),key=lambda id:(next(j for j in jobs if j['id']==id)['priority'],id))
        if not ready:raise ValueError('dependency cycle')
        order.extend(ready);todo.difference_update(ready)
    for j in jobs:
        for d in j['dependencies']:
            assert d[9:] in clips if d.startswith('baseline:') else d in ids or d in DONE,d
    write(out/'config/jobs.json',jobs);write(out/'config/run_order.json',order)
    write(out/'config/first_frame_reviews.json',{j['id']:{'approved':False,'identity_mapping_checked':False,'action_ready':False,'notes':'接手 Agent 实际看图后填写，不要求人类逐条批准。'} for j in jobs})
    write(out/'config/cuts.json',{j['id']:{'video':f'runs/{j["id"]}/attempt_01/source.mp4','reviewed':False,'start_seconds':None,'end_seconds_exclusive':None,'loop':j['loop'],'event_times':{},'notes':''} for j in jobs})
    sync=read(old/'integration/sync_groups.json')
    sync.append({'id':'G10','clips':['C07_linwan_stamp','C08_suyao_fall_kneel'],'event':'foot_contact_then_loss_of_balance','note':'当前 C07 已重定时，检查接触源帧误差，最大0.1秒；允许用现有源视频重截，不能凭播放起点判定命中。苏瑶朝左。'})
    write(out/'integration/sync_groups.json',sync)
    beatmap=[]
    story=read(CONTENT/'story.json')
    beats=story['beats']
    for i,b in enumerate(beats,1):
        beatmap.append({'beat':i,'title':b['title'],'scene':b['scene'],'required_animation_ids':[j['id'] for j in jobs if i in j['beats']],'current_actors':b['actors'],'current_props':b.get('props',[])})
    write(out/'integration/beat_assets.json',beatmap)
    write(out/'config/scope.json',{'base_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'required_count':len(jobs),'required_ids':[j['id'] for j in jobs],'reuse_delivered':sorted(DONE),'reuse_required_extras':['E01_extra_f_idle','E02_extra_f_walk','E03_extra_m_idle'],'repair_extra_walk':'E04_extra_m_walk','quality_review':['C07 contact timing and G10 composite; repair source cut or regenerate only if review fails'],'prototype_only':[k for k in clips if k.startswith(('W_','H_'))],'retired':['C01_linwan_tear_photo','S05_suyao_kneel_idle'],'out_of_scope':['new BGM','dialogue recording','regenerate existing static props or masters'],'status':'handoff_prepared_not_generated'})
    # Keep mutable production state and visual acceptance separate from historical partial reports.
    write(out/'config/delivery_reviews.json',{j['id']:{'passed':False,'reviewer':'','evidence':[],'notes':'','checks':{k:False for k in ['identity','no_intro','registration','matte','expression','endpoints','anchors','prop_contact','forward_reverse_seek','loop_three_cycles_or_once_hold']}} for j in jobs})
    write(out/'config/sync_reviews.json',{g['id']:{'passed':False,'evidence':[],'notes':''} for g in sync})
    for tool in ['validate_round2.py','prepare_review.py']:
        copyfile(ROOT/'Tools/SuperstarAnimationDelivery'/tool,out/'tools'/tool)
    with zipfile.ZipFile(ROOT/'Deliverables/Superstar-MissingAssets-partial-20261115.zip') as z:
        for name in z.namelist():
            if name.startswith('output/runs/C07_linwan_stamp/') or name=='output/animations/C07_linwan_stamp/animation.json':
                if not name.endswith('/'):
                    target=out/'review_sources'/name
                    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(z.read(name))
    copyfile(ROOT/'Docs/SuperstarStory/Animation-Round2-20261115.md',out/'START_HERE.md')
    copyfile(ROOT/'Tools/SuperstarAnimationDelivery/交付契约.md',out/'docs/交付契约.md')
    write(out/'交给生成Agent.txt','请从 START_HERE.md 开始，按 config/run_order.json 完成78项必做动画及同步组验收。包内有当前素材、身份参考、首帧/视频提示词与ComfyUI API工作流。先预检再单条试跑；不需要原电脑文件。不要把临时走路图或单帧姿态报告为正式动作完成。遇失败按任务记录并继续其他可执行任务，最终交付真实完成项及欠件清单。\n')
    rows=['# 第二批动画补件清单','',f'必做 {len(jobs)} 项；其中21项表情。已交付8段复用，6个临时走路循环和4个单帧持物姿态只作对照。','', '| ID | 动作 | 优先级 | 段落 | 循环 | 依赖 |','|---|---|---|---|---|---|']
    rows += [f'|{j["id"]}|{j["name"]}|{j["priority"]}|{j["beats"]}|{j["loop"]}|{", ".join(j["dependencies"])}|' for j in jobs]
    write(out/'生成总表.md','\n'.join(rows)+'\n')
    write(out/'MANIFEST.sha256.json',{p.relative_to(out).as_posix():sha(p) for p in sorted(out.rglob('*')) if p.is_file()})


def main():
    destination=ROOT/'Deliverables'/f'{NAME}.zip'
    with tempfile.TemporaryDirectory(prefix='round2-',dir=ROOT/'Saved') as temp:
        temp=Path(temp);old=temp/'old';old.mkdir()
        with zipfile.ZipFile(BASE) as z:
            for info in z.infolist():
                path=Path(info.filename)
                assert not path.is_absolute() and '..' not in path.parts
            z.extractall(old)
        out=temp/NAME;out.mkdir();build(out,old/'Superstar-MissingAssets-20261115')
        subprocess.run([__import__('sys').executable,str(out/'tools/validate_round2.py'),'handoff'],check=True)
        with zipfile.ZipFile(destination,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
            for p in sorted(out.rglob('*')):
                if p.is_file() and '__pycache__' not in p.parts:z.write(p,NAME+'/'+p.relative_to(out).as_posix())
    write(destination.with_suffix('.zip.sha256.txt'),sha(destination)+'  '+destination.name+'\n')
    print(destination, destination.stat().st_size)

if __name__=='__main__':main()
