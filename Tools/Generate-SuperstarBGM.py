"""Original deterministic instrumental score. Uses synthesized instruments, no samples/model.
Writes loop-ready stereo 48 kHz PCM16 WAVs and a complete beat-to-cue manifest.
"""
from pathlib import Path
import hashlib, json, wave
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Content/SuperstarStory/Audio'
SR = 48000
# A shared four-note motif ties the drama together; mode, rhythm and orchestration change.
CUES = [
    ('set_pressure', '镜头背后', 80, 50, 'minor', .55, [1,2,3,4]),
    ('rain_secrets', '路灯下的雨', 72, 50, 'minor', .12, [5,6,32]),
    ('backstage_intrigue', '甜笑与暗流', 88, 50, 'minor', .42, list(range(7,20))+[21,22,23]),
    ('spotlight', '灯光下的裂缝', 100, 50, 'minor', .8, [20,29,30,31]),
    ('mirror_doubt', '镜中疑云', 76, 50, 'minor', .22, [24,25,26,27,28]),
    ('truth_live', '让证据说话', 96, 50, 'minor', .65, [33,34,35,36]),
    ('afterglow', '落幕之后', 80, 50, 'major', .16, [37,38,39]),
]

def instrument(note, duration, kind, rng):
    t = np.arange(round(duration*SR), dtype=np.float64)/SR
    f = 440*2**((note-69)/12)
    if kind == 'piano':
        x = sum(a*np.sin(2*np.pi*f*h*t)*np.exp(-t*(1.0+h*.55))
                for h,a in [(1,1),(2,.34),(3,.16),(4,.08),(7,.025)])
        env = (1-np.exp(-t*140)) * np.minimum(1,(duration-t)/.12)
    elif kind == 'bell':
        x = (np.sin(2*np.pi*f*t)+.22*np.sin(2*np.pi*f*2*t)*np.exp(-t*3))
        env = (1-np.exp(-t*90))*np.exp(-t*1.7)*np.minimum(1,(duration-t)/.15)
    elif kind == 'pad':
        x = sum(np.sin(2*np.pi*f*h*t + .006*h*np.sin(2*np.pi*.3*t))/h**2.2 for h in range(1,5))
        x += .20*np.sin(2*np.pi*f*1.0018*t)
        env = np.sin(np.pi*np.clip(t/duration,0,1))**1.5
    else:
        x = np.sin(2*np.pi*f*t)+.18*np.sin(4*np.pi*f*t)
        env = (1-np.exp(-t*60))*np.exp(-t*1.8)*np.minimum(1,(duration-t)/.1)
    return (x*env).astype(np.float32)

def render(cue):
    name,title,bpm,root,mode,energy,beats = cue
    rng=np.random.default_rng(20261115+CUES.index(cue))
    beat=60/bpm; duration=64*beat; n=round(duration*SR)
    dry=np.zeros((n,2),dtype=np.float32)
    def add(x, at, gain, pan=0):
        start=round(at*SR)%n
        stereo=x[:,None]*np.array([np.sqrt((1-pan)/2),np.sqrt((1+pan)/2)],dtype=np.float32)*gain
        # Wrap note releases over the seam; the last bar is not faded to silence.
        split=min(len(x),n-start)
        dry[start:start+split]+=stereo[:split]
        if split<len(x):dry[:len(x)-split]+=stereo[split:]
    progression = [[0,3,7,14],[-2,2,7,12],[-5,0,3,10],[-7,-3,2,9]] if mode=='minor' else [[0,4,7,14],[-5,0,4,7],[-3,0,4,7],[-7,-3,2,9]]
    motif=[7,10,9,2] if mode=='minor' else [7,11,9,4]
    for bar in range(16):
        chord=progression[bar%4]; time=bar*4*beat
        for k,pitch in enumerate(chord):
            add(instrument(root+pitch,5.8*beat,'pad',rng),time,.048,(-.55+.36*k))
        add(instrument(root+chord[0]-12,3.8*beat,'bass',rng),time,.13)
        for k,step in enumerate([0,1.5,2.5] if energy<.3 else [0,.75,1.5,2,2.75,3.5]):
            pitch=root+12+chord[(k+bar//4)%4]
            add(instrument(pitch,2.8*beat,'piano',rng),time+step*beat,.11*(.86+.14*rng.random()),-.25 if k%2 else .25)
        if bar%2==0:
            add(instrument(root+24+motif[(bar//2)%4],3.4*beat,'bell',rng),time+.5*beat,.08 if energy<.5 else .065,.15)
        if energy>.3:
            for step in [0,2]:
                t=np.arange(round(.24*SR))/SR
                kick=np.sin(2*np.pi*(44*t+2.4*(1-np.exp(-t*30))))*np.exp(-t*22)*(1-np.exp(-t*250))
                add(kick.astype(np.float32),time+step*beat,.10*energy)
            for step in [1,3]:
                noise=rng.normal(0,1,round(.12*SR)).astype(np.float32)
                # Quiet brushed pulse; no sharp cinematic hits over dialogue.
                noise=np.convolve(noise,np.ones(9)/9,mode='same').astype(np.float32)
                t=np.arange(len(noise))/SR
                add(noise*np.sin(np.pi*t/.12)**2*np.exp(-t*25),time+step*beat,.10*energy,-.2)
    wet=dry.copy()
    for seconds,gain in [(.113,.16),(.227,.12),(.389,.09),(.617,.065),(.947,.04)]:
        wet += np.roll(dry[:,::-1],round(seconds*SR),axis=0)*gain
    wet -= wet.mean(axis=0)
    rms=float(np.sqrt(np.mean(wet**2)))
    wet *= min(.105/max(rms,1e-9),.65/np.max(np.abs(wet)))
    peak=float(np.max(np.abs(wet)))
    pcm=np.rint(np.clip(wet,-.999,.999)*32767).astype('<i2')
    path=OUT/(name+'.wav')
    with wave.open(str(path),'wb') as w:
        w.setparams((2,2,SR,len(pcm),'NONE','not compressed'));w.writeframes(pcm.tobytes())
    seam=float(np.max(np.abs(wet[0]-wet[-1])))
    assert seam<.025,(name,seam)
    return dict(id=name,title=title,file='Audio/'+path.name,bpm=bpm,bars=16,duration_seconds=duration,
                beats=beats,peak_dbfs=round(20*np.log10(peak),2),rms_dbfs=round(20*np.log10(np.sqrt(np.mean(wet**2))),2),
                seam_step=seam,sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    tracks=[render(c) for c in CUES]
    mapping={str(b):t['id'] for t in tracks for b in t['beats']}
    assert sorted(map(int,mapping))==list(range(1,40))
    manifest=dict(version=1,origin='Original algorithmic composition and instrument synthesis; no third-party samples or AI music model.',
                  seed=20261115,sample_rate=SR,channels=2,bits_per_sample=16,volume=.48,crossfade_seconds=1.4,
                  pause_policy='Pause music with explicit story pause; continue loops while waiting for evidence.',tracks=tracks,beat_to_track=mapping)
    (OUT/'music.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2,default=float)+'\n',encoding='utf-8')
    for t in tracks:print(t['id'],round(t['duration_seconds'],2),'seconds; peak',t['peak_dbfs'],'dBFS; seam',round(t['seam_step'],6))

if __name__=='__main__':main()
