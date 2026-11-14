"""Validate source cues and, with --recording, the real UE master-output smoke capture."""
from pathlib import Path
import argparse, hashlib, json, wave
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(__doc__)
p.add_argument('--recording',type=Path)
args=p.parse_args()

def read(path):
    with wave.open(str(path),'rb') as w:
        assert w.getnchannels()==2 and w.getsampwidth()==2 and w.getframerate()==48000,path
        x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').reshape(-1,2).astype(np.float64)/32768
    return x

manifest=json.loads((ROOT/'Content/SuperstarStory/Audio/music.json').read_text(encoding='utf-8'))
assert sorted(map(int,manifest['beat_to_track']))==list(range(1,40))
assert len(manifest['tracks'])==7
for t in manifest['tracks']:
    path=ROOT/'Content/SuperstarStory'/t['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==t['sha256']
    x=read(path)
    assert abs(len(x)/48000-t['duration_seconds'])<1/48000
    assert .03<np.sqrt(np.mean(x*x))<.2 and np.max(np.abs(x))<.9
    assert np.max(np.abs(x[0]-x[-1]))<.025
    assert max(np.abs(x.mean(axis=0)))<.0001
    # No silent bar at the seam or elsewhere, and each clip genuinely varies.
    blocks=[float(np.sqrt(np.mean(v*v))) for v in np.array_split(x,32)]
    assert min(blocks)>.005 and np.std(x)>.01
print('SOURCE PASS: 7 original stereo 48k PCM16 cues; 39 beats covered; hashes, headroom, DC, duration and loop seams valid.')

if args.recording:
    x=read(args.recording)
    assert 85<len(x)/48000<88,('capture duration',len(x)/48000)
    def rms(a,b):return float(np.sqrt(np.mean(x[int(a*48000):int(b*48000)]**2)))
    for a,b in [(3,4),(49,50),(54.5,55.5),(58.5,59.5),(62,63),(66,67),(70,71),(74,75),(78,79),(82,83)]:
        assert rms(a,b)>.002,(a,b,'unexpected silence',rms(a,b))
    for a,b in [(52.7,53.5),(56.7,57.5),(84.7,85.5)]:
        assert rms(a,b)<.0001,(a,b,'pause/mute/cleanup failed',rms(a,b))
    first=x[2*48000:3*48000].ravel();loop=x[50*48000:51*48000].ravel()
    corr=float(np.corrcoef(first,loop)[0,1])
    assert corr>.99,('48-second runtime loop correlation',corr)
    assert np.max(np.abs(x))<.95
    print(f'ENGINE AUDIO PASS: {len(x)/48000:.2f}s actual master mix; all 7 cues audible, pause/mute/cleanup silent; loop correlation={corr:.6f}; no clipping.')
