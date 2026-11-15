"""Pack generated sprite sheets and their measured hand points into game frames.
Only cell extraction, uniform scaling and transparent atlas placement; no painted edits.
Run with --source pointing to the built-in imagegen output folder, or saved ArtSource folder.
"""
from pathlib import Path
import argparse, json, hashlib, shutil
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Content/SuperstarStory'
STAMP='Motion20261115'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def save(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(__doc__);ap.add_argument('--source',type=Path,required=True);args=ap.parse_args()
    sources=read(ROOT/'Tools/superstar-motion-sources.json');clips=read(OUT/'clips.json')
    art=ROOT/'ArtSource/SuperstarMotion20261115';art.mkdir(parents=True,exist_ok=True)
    for role,info in sources.items():
        src=args.source/info['source'];dst=art/info['source']
        if src.resolve()!=dst.resolve():shutil.copy2(src,dst)
        info['sha256']=digest(dst)
    save(art/'sources.json',sources)
    # Measured source-cell hand coordinates, as fractions of cell size.
    hands={'linwan':[(.612,.347),(.612,.347),(.622,.347),(.617,.347),(.612,.344),(.614,.344),(.571,.344),(.608,.281)],
           'xiaobei':[(.603,.352),(.603,.352),(.603,.352),(.603,.352),(.603,.352),(.603,.352),(.603,.352),(.603,.352)],
           'chenmo':[(.65,.416)]*6,'xiaozhou':[(.707,.389)]*6,'actress':[(.65,.377)]*6,
           'suyao':[(.70,.46)]*6,'suyaohold':[(.426,.43)]}
    for role in ['linwan','xiaobei','suyao','chenmo','xiaozhou','actress','suyaohold']:
        im=Image.open(art/sources[role]['source']).convert('RGBA')
        cols=4 if role in ['linwan','xiaobei'] else (1 if role=='suyaohold' else 3)
        rows=1 if role=='suyaohold' else 2
        cells=[];boxes=[]
        for j in range(cols*rows):
            cell=im.crop((round(j%cols*im.width/cols),round(j//cols*im.height/rows),round((j%cols+1)*im.width/cols),round((j//cols+1)*im.height/rows)))
            mask=np.asarray(cell)[:,:,3]>128;ys,xs=np.nonzero(mask)
            boxes.append((xs.min(),ys.min(),xs.max()+1,ys.max()+1));cells.append(cell)
        scale=478/float(np.median([b[3]-b[1] for b in boxes]))
        packed=[];anchors=[]
        for j,(cell,box) in enumerate(zip(cells,boxes)):
            a=np.asarray(cell)[:,:,3]>128
            crown=np.nonzero(a[box[1]:box[1]+35])[1]
            center=float(np.median(crown))
            resized=cell.resize((round(cell.width*scale),round(cell.height*scale)),Image.Resampling.LANCZOS)
            x=round(320-center*scale);y=round(570-box[3]*scale)
            frame=Image.new('RGBA',(640,640));frame.alpha_composite(resized,(x,y));packed.append(frame)
            hx,hy=hands[role][j];anchors.append([round(x+hx*cell.width*scale,2),round(y+hy*cell.height*scale,2)])
        groups=[('W_'+role,list(range(6)),True)] if role!='suyaohold' else [('H_suyao',[0],False)]
        if role in ['linwan','xiaobei']:groups.append(('H_'+role,[6],False))
        if role=='linwan':groups.append(('H_linwan_phone',[7],False))
        for id,indices,loop in groups:
            path=f'Animations/{STAMP}/{id}';dest=OUT/path;dest.mkdir(parents=True,exist_ok=True)
            hashes=[]
            for i,j in enumerate(indices,1):
                target=dest/f'{id}_{i:06d}.png';packed[j].save(target);hashes.append(digest(target))
            clips[id]=dict(path=path,frames=len(indices),fps=5,loop=loop,pivot=[.5,.890625],sha256=hashes,
                screen_facing='left' if role=='suyaohold' else 'right',hand_anchors=[anchors[j] for j in indices],
                kind='generated_walk' if loop else 'generated_hold',source_delivery=STAMP)
    stat=OUT/'Static'/STAMP;stat.mkdir(parents=True,exist_ok=True)
    for role in ['phone','tripod','ev01','ev02']:shutil.copy2(art/sources[role]['source'],stat/f'{role}.png')
    save(OUT/'clips.json',clips)
    print('Packed 6 walk cycles, 4 holding poses, 4 new scene images; originals preserved.')

if __name__=='__main__':main()
