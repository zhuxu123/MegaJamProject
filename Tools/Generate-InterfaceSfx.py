"""Original deterministic interface foley, synthesized from oscillators and seeded noise; no samples."""
from pathlib import Path
import hashlib,json,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'ArtSource/InterfaceSfx';SR=48000
OUT.mkdir(parents=True,exist_ok=True);rng=np.random.default_rng(2026111507)
def time(d):return np.arange(round(SR*d))/SR
def env(t,d,attack=.004,release=.035):return np.minimum(1,t/attack)*np.minimum(1,(d-t)/release)
def bell(f,d):
 t=time(d);return (np.sin(2*np.pi*f*t)*np.exp(-t*5)+.25*np.sin(2*np.pi*f*2.006*t)*np.exp(-t*12)+.08*np.sin(2*np.pi*f*3.98*t)*np.exp(-t*21))*env(t,d,.002,.09)
def add(out,x,start,gain=1):
 at=round(start*SR);n=min(len(x),len(out)-at);out[at:at+n]+=x[:n]*gain
sounds={}
x=np.zeros(round(.23*SR));add(x,bell(1174.66,.14),0,.7);add(x,bell(1760,.16),.055,.4);sounds['chapter']=x
for name,reverse in [('forward',False),('rewind',True)]:
 d=.42;t=time(d);phase=2*np.pi*(420*t+900*t*t)
 x=(.30*np.sin(phase)+.12*np.sin(1.997*phase)+.10*rng.normal(size=len(t)))
 # Soft tape flutter in two bursts; mirrored envelope/pitch makes the direction audible.
 x*=np.sin(np.pi*t/d)**1.6*(.65+.35*np.cos(2*np.pi*16*t))
 sounds[name]=x[::-1].copy() if reverse else x
x=np.zeros(round(.24*SR))
for at,base,amp in [(0,1700,1),(.043,780,.8),(.081,340,.35)]:
 t=time(.095);noise=rng.normal(size=len(t));noise=noise-np.convolve(noise,np.ones(15)/15,mode='same')
 click=(noise*.40+np.sin(2*np.pi*base*t)*.4)*np.exp(-t*100)*env(t,.095,.0006,.02)
 add(x,click,at,amp)
sounds['shutter']=x
t=time(.20);noise=rng.normal(size=len(t));noise=np.convolve(noise,np.ones(25)/25,mode='same')
sounds['pan']=(.20*np.sin(2*np.pi*150*t)+.05*np.sin(2*np.pi*310*t)+noise*.30)*np.sin(np.pi*t/.20)**2
for name,up in [('zoom_in',True),('zoom_out',False)]:
 t=time(.26);f0,f1=(330,720) if up else (720,330)
 phase=2*np.pi*(f0*t+(f1-f0)*t*t/(2*.26));x=(np.sin(phase)*.4+np.sin(phase*2)*.08)*env(t,.26,.018,.055)
 sounds[name]=x
x=np.zeros(round(1.85*SR))
for at,f,g in [(0,587.33,.8),(.19,880,.65),(.40,739.99,.75),(.65,1174.66,.9)]:add(x,bell(f,1.15),at,g)
add(x,bell(293.665,1.12),.65,.19);sounds['result']=x
manifest={'generator':'Tools/Generate-InterfaceSfx.py','source':'Original oscillator/noise synthesis; no third-party samples or models','sample_rate':SR,'channels':2,'format':'PCM16','cues':[]};preview=[]
for name,x in sounds.items():
 x=x-x.mean();x*=env(time(len(x)/SR),len(x)/SR,.001,.015)
 x*=.50/max(np.max(np.abs(x)),1e-8) # -6.02 dBFS source peak, runtime gain adds headroom
 stereo=np.column_stack([x,x]);pcm=np.rint(stereo*32767).astype('<i2')
 path=OUT/(name+'.wav')
 with wave.open(str(path),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(SR);f.writeframes(pcm.tobytes())
 manifest['cues'].append({'id':name,'file':path.name,'asset':'/Game/Audio/Interface/SFX_'+name,'seconds':len(x)/SR,'peak_dbfs':float(20*np.log10(np.max(np.abs(pcm.astype(float)))/32768)),'rms_dbfs':float(20*np.log10(np.sqrt(np.mean((pcm.astype(float)/32768)**2)))),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
 preview.extend([stereo*.65,np.zeros((round(SR*.35),2))])
with wave.open(str(OUT/'preview_all.wav'),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(SR);f.writeframes(np.rint(np.concatenate(preview)*32767).astype('<i2').tobytes())
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Generated 8 original stereo48k PCM16 cues and preview:',OUT)
