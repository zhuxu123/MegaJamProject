"""Derive head-top anchors from the visible crown of each current 640px sprite.
These clips have no props above the head. Future overhead poses need reviewed anchors.
Only writes metadata; preserves all original animation pixels.
"""
from pathlib import Path
import json
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]/'Content/SuperstarStory'
anchors={}
for folder in sorted((root/'Animations').iterdir()):
    frames=[]
    for path in sorted(folder.glob('*.png')):
        a=np.array(Image.open(path))
        mask=a[:,:,3]>128
        rows=np.flatnonzero(mask.sum(axis=1)>=8)
        assert len(rows),path
        top=int(rows[0])
        band=mask[top:min(top+55,640)]
        xs=np.nonzero(band)[1]
        frames.append([round(float(np.median(xs)),2),top])
    anchors[folder.name]=frames
(root/'head_anchors.json').write_text(json.dumps(anchors,indent=2)+'\n',encoding='utf-8')
assert anchors['C03_xiaobei_fall_scatter'][-1][1]>anchors['C03_xiaobei_fall_scatter'][0][1]+100
assert anchors['C09_suyao_rise'][-1][1]<anchors['C09_suyao_rise'][0][1]-100
print('HEAD ANCHORS PASS:',sum(map(len,anchors.values())),'frames, falling/rising height changes verified')
