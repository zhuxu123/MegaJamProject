"""Derive head-top anchors from the visible crown of each current 640px sprite.
Current character frames have no props above the head. Separate scene props are excluded.
Only writes metadata; preserves all original animation pixels.
"""
from pathlib import Path
import json
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]/'Content/SuperstarStory'
anchors={}
clips=json.loads((root/'clips.json').read_text(encoding='utf-8'))
for id,clip in clips.items():
    frames=[]
    for i in range(1,clip['frames']+1):
        path=root/clip['path']/f'{id}_{i:06d}.png'
        a=np.array(Image.open(path))
        mask=a[:,:,3]>128
        rows=np.flatnonzero(mask.sum(axis=1)>=8)
        assert len(rows),path
        top=int(rows[0])
        band=mask[top:min(top+55,640)]
        xs=np.nonzero(band)[1]
        frames.append([round(float(np.median(xs)),2),top])
    anchors[id]=frames
(root/'head_anchors.json').write_text(json.dumps(anchors,indent=2)+'\n',encoding='utf-8')
assert anchors['C03_xiaobei_fall_scatter'][-1][1]>anchors['C03_xiaobei_fall_scatter'][0][1]+100
assert anchors['C09_suyao_rise'][-1][1]<anchors['C09_suyao_rise'][0][1]-100
print('HEAD ANCHORS PASS:',sum(map(len,anchors.values())),'frames, falling/rising height changes verified')
