"""Create 5FPS QA previews and an UNREVIEWED anchor template after export; does not infer hand/head locations."""
import argparse,json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(__doc__);p.add_argument('id');a=p.parse_args()
jobs=json.loads((ROOT/'config/jobs.json').read_text(encoding='utf-8-sig'));job=next(j for j in jobs if j['id']==a.id)
base=ROOT/'output/animations'/a.id
meta=json.loads((base/'animation.json').read_text(encoding='utf-8-sig'))
frames=[]
for m in meta['frame_mapping']:
    with Image.open(base/m['file']) as im:
        bg=Image.new('RGBA',(640,640),(35,40,48,255));bg.alpha_composite(im.convert('RGBA'));frames.append(bg.convert('RGB'))
for name,seq in [('preview_reverse.gif',list(reversed(frames))),('preview_three_cycles.gif',frames*3 if job['loop'] else frames+[frames[-1]]*5)]:
    target=base/name
    if target.exists():raise FileExistsError(target)
    seq[0].save(target,save_all=True,append_images=seq[1:],duration=200,disposal=2,optimize=False)
target=base/'anchors.json'
if not target.exists():
    target.write_text(json.dumps({'id':a.id,'screen_facing':job['screen_facing'],'prop_mode':job['prop_contract']['mode'],'coordinates':'unmirrored source pixels; frame is one-based','frames':[{'frame':i+1,'head':None,'hand':None,'hand_left':None,'hand_right':None,'notes':'MEASURE_VISUALLY; do not approve null anchors'} for i in range(len(frames))]},ensure_ascii=False,indent=2),encoding='utf-8')
print('QA previews created. Measure anchors and inspect source, forwards/reverse, endpoints, prop composite and loop seams before recording review.')
