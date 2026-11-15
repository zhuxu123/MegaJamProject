"""Validate the portable handoff or actual produced animations. No visual result is inferred from file existence."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import sys
import zipfile
from PIL import Image

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def local(base,name):
    p=(base/name).resolve()
    if not p.is_relative_to(base.resolve()):raise ValueError('Path escapes package: '+str(name))
    return p
def require(value,message):
    if not value:raise ValueError(message)
def point(value):return isinstance(value,list) and len(value)==2 and all(isinstance(x,(float,int)) and math.isfinite(x) and 0<=x<640 for x in value)

def handoff():
    manifest=read(ROOT/'MANIFEST.sha256.json')
    for name,sha in manifest.items():
        p=local(ROOT,name);require(p.is_file() and digest(p)==sha,'Manifest mismatch: '+name)
    jobs=read(ROOT/'config/jobs.json');clips=read(ROOT/'current/clips.json');chars=read(ROOT/'config/characters.json');ids={j['id'] for j in jobs}
    require(len(ids)==len(jobs)==read(ROOT/'config/scope.json')['required_count'],'Task count/duplicates')
    for c in chars:require(local(ROOT,c['reference']).is_file(),'Missing identity reference: '+c['id'])
    spec=importlib.util.spec_from_file_location('comfy_jobs',ROOT/'tools/comfy_jobs.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    order=read(ROOT/'config/run_order.json');available=set(clips)
    require(set(order)==ids and len(order)==len(ids),'Run order mismatch')
    for id in order:
        j=next(j for j in jobs if j['id']==id)
        require(j['canvas']==[640,640] and j['export_fps']==5,'Export contract: '+id)
        for d in j['dependencies']:require((d[9:] if d.startswith('baseline:') else d) in available,'Unresolved dependency: '+d)
        for name in ['reference','prompt','first_frame_prompt']:require(local(ROOT,j[name]).is_file(),'Missing '+j[name])
        prompt=(ROOT/j['prompt']).read_text(encoding='utf-8')
        require(prompt.startswith('For the target video, at 0.00 seconds into the target video, <Picture 1> (from [Shot 1]) is fully referenced.'),'H3 prefix: '+id)
        require(all(k in prompt for k in ['integrated_multimodal_description: [Shot 1]','overall_soundscape:','non_diegetic_music:']),'H3 fields: '+id)
        mod.make_graph(j,1,'superstar_handoff/'+id+'.png')
        available.add(id)
    for id,c in clips.items():
        frames=sorted(local(ROOT/'current',c['path']).glob('*.png'))
        require(len(frames)==c['frames'],'Current reference frames: '+id)
        require([digest(p) for p in frames]==c['sha256'],'Current reference checksum: '+id)
    print(f'PASS handoff: {len(manifest)} files, {len(jobs)} tasks, all dependency references and offline API plans. No generation performed.')


def delivery(ids):
    jobs=read(ROOT/'config/jobs.json');allids={j['id'] for j in jobs};selected=set(ids) if ids else allids
    require(bool(selected) and selected<=allids,'Unknown/empty delivery IDs')
    reviews=read(ROOT/'config/delivery_reviews.json');sources=set();files=set();failures=[]
    for j in jobs:
        id=j['id']
        if id not in selected:continue
        try:
            base=ROOT/'output/animations'/id;meta=read(base/'animation.json');a=read(base/'anchors.json');review=reviews[id]
            require(meta['id']==id and meta['fps']==5 and meta['loop']==j['loop'],'Animation identity/fps/loop')
            require(meta['canvas']==[640,640] and meta['anchor_pixels']==[320,570],'Canvas/pivot')
            mapping=meta['frame_mapping'];n=len(mapping)
            require(n==meta['frame_count'] and n>=2,'Must contain an animation, not one static frame')
            require(abs(meta['playback_seconds']-n/5)<1e-6,'Duration')
            source=local(ROOT,meta['source']);require(source.is_file() and digest(source)==meta['source_sha256'],'Source missing/hash changed')
            sources.add(source)
            cut=meta['cut'];require(cut.get('reviewed') is True and 0<=cut['start_seconds']<cut['end_seconds_exclusive'],'Unreviewed cut')
            require(a.get('screen_facing')==j['screen_facing'] and a.get('prop_mode')==j['prop_contract']['mode'],'Facing/prop-mode contract')
            require(len(a['frames'])==n,'Per-frame anchors')
            hashes=[];times=[];sourceframes=[]
            for i,(m,anchors) in enumerate(zip(mapping,a['frames']),1):
                require(m['output_frame']==i and abs(m['output_seconds']-(i-1)/5)<1e-6,'Frame ordering/time')
                require(anchors['frame']==i and point(anchors['head']),'Head anchor')
                if j['prop_contract']['mode']!='none':require(point(anchors['hand']),'Active hand anchor')
                t=m['actual_source_seconds'];require(cut['start_seconds']<=t<cut['end_seconds_exclusive'],'Frame outside valid action')
                require(abs(t-m['requested_source_seconds'])<=.1+1e-6,'5FPS source sample error')
                times.append(t);sourceframes.append(m['source_frame'])
                p=local(base,m['file'])
                with Image.open(p) as im:
                    require(im.size==(640,640) and im.mode=='RGBA','PNG canvas/RGBA')
                    lo,hi=im.getchannel('A').getextrema();require(lo==0 and hi>0,'Transparent background and visible subject')
                hashes.append(digest(p));files.add(p)
            require(all(b>a for a,b in zip(times,times[1:])) and len(set(sourceframes))==n,'Duplicate or reversed source samples')
            require(len(set(hashes))>1,'Static image repeated as animation')
            require(set(j['events'])<=set(meta['events']),'Missing action events')
            for event in meta['events'].values():
                f=event['output_frame'];require(1<=f<=n,'Event output frame')
                require(abs(times[f-1]-event['source_seconds'])<=.1+1e-6,'Contact/event differs by more than 0.1s')
            require(review.get('passed') is True and bool(review.get('reviewer')) and review.get('checks') and all(v is True for v in review['checks'].values()),'Visual review incomplete')
            needed={'identity','no_intro','registration','matte','expression','endpoints','anchors','prop_contact','forward_reverse_seek','loop_three_cycles_or_once_hold'}
            require(needed<=set(review['checks']),'Visual review checks missing')
            require(bool(review['evidence']),'Missing visual evidence')
            for evidence in review['evidence']:
                p=local(ROOT,evidence);require(p.is_file(),'Missing review evidence');files.add(p)
            if j['loop']:require(hashes[0]!=hashes[-1],'Duplicate first/last cycle endpoint')
            for name in ['preview_5fps.gif','preview_reverse.gif','preview_three_cycles.gif','animation.json','anchors.json']:
                p=base/name;require(p.is_file(),'Missing '+name);files.add(p)
        except (ValueError,KeyError,OSError,TypeError) as e:failures.append(id+': '+str(e))
    sync=read(ROOT/'config/sync_reviews.json');groups=read(ROOT/'integration/sync_groups.json')
    for g in groups:
        if selected==allids or (set(g['clips'])&selected and all(id in selected or id not in allids for id in g['clips'])):
            r=sync[g['id']]
            if r.get('passed') is not True or not r.get('evidence'):failures.append(g['id']+': sync review incomplete')
            for name in r.get('evidence',[]):
                p=local(ROOT,name)
                if not p.is_file():failures.append(g['id']+': missing evidence '+name)
                else:files.add(p)
    report={'selected':sorted(selected),'remaining':sorted(allids-selected),'passed':not failures,'scope':'complete' if selected==allids else 'partial','pending_sync_groups':[g['id'] for g in groups if not sync[g['id']].get('passed')],'failures':failures,'note':'Automated checks require separately recorded visual review; they do not judge aesthetics.'}
    (ROOT/'delivery_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    for failure in failures:print('FAIL '+failure)
    require(not failures,f'{len(failures)} delivery failures; see delivery_report.json')
    print(f'PASS {report["scope"]} delivery: {len(selected)} animations; still missing {len(allids-selected)}.')
    # Include full per-clip output, attempt provenance, source videos, submitted graphs and first frames.
    for id in selected:
        files.update(p for p in (ROOT/'output/animations'/id).rglob('*') if p.is_file())
        files.update(p for p in (ROOT/'runs'/id).rglob('*') if p.is_file())
    files.update(sources)
    for folder in ['config','integration','workflows']:
        files.update(p for p in (ROOT/folder).rglob('*') if p.is_file())
    files.add(ROOT/'delivery_report.json')
    return files,report


def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('command',choices=['handoff','delivery','pack']);p.add_argument('--ids',nargs='+');args=p.parse_args()
    if args.command=='handoff':handoff();return
    files,report=delivery(args.ids)
    if args.command=='pack':
        require((ROOT/'config/environment.json').is_file(),'Missing config/environment.json')
        require((ROOT/'integration/FAILED.md').is_file(),'Missing integration/FAILED.md')
        dest=ROOT/('Superstar-Animation-Round2-'+report['scope']+'-delivery.zip')
        manifest={f.relative_to(ROOT).as_posix():digest(f) for f in sorted(files)}
        with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for f in sorted(files):z.write(f,f.relative_to(ROOT).as_posix())
            z.writestr('DELIVERY.sha256.json',json.dumps(manifest,ensure_ascii=False,indent=2))
        print(dest)

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError,TypeError) as e:print('ERROR:',e,file=sys.stderr);sys.exit(1)
