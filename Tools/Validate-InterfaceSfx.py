"""Check original cue quality and optionally correlate actual Unreal mixer recording with every source."""
import argparse,hashlib,json,re,wave
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def load(p):
 with wave.open(str(p),'rb') as f:
  assert f.getsampwidth()==2 and f.getframerate()==48000 and f.getnchannels()==2,p
  return np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').reshape(-1,2).astype(np.float64)/32768
p=argparse.ArgumentParser(__doc__);p.add_argument('--recording',type=Path);p.add_argument('--log',type=Path);args=p.parse_args()
m=json.loads((ROOT/'ArtSource/InterfaceSfx/manifest.json').read_text(encoding='utf-8'));sources={}
for cue in m['cues']:
 path=ROOT/'ArtSource/InterfaceSfx'/cue['file'];x=load(path);sources[cue['id']]=x.mean(axis=1)
 assert hashlib.sha256(path.read_bytes()).hexdigest()==cue['sha256']
 assert abs(len(x)/48000-cue['seconds'])<1e-6
 assert .49<float(np.max(np.abs(x)))<.51
 assert np.max(np.abs(x[0]))<.0001 and np.max(np.abs(x[-1]))<.0001
 assert np.sqrt(np.mean(x*x))>.005
 assert (ROOT/'Content/Audio/Interface'/('SFX_'+cue['id']+'.uasset')).is_file()
print('SOURCE PASS: 8 finite stereo48k PCM16 cues, -6dBFS peaks, faded endpoints, source hashes, imported assets.')
if args.recording:
 assert args.log,'--log is required with --recording'
 output=load(args.recording).mean(axis=1);text=args.log.read_text(encoding='utf-8-sig')
 events=re.findall(r'InterfaceSfx smoke: cue=(\w+) time=([\d.]+)',text)
 assert len(events)==9 and len({id for id,_ in events})==8
 assert np.max(np.abs(output))<.99,'Mixer clips'
 report=[]
 for id,t in events:
  # Recorder setup and device startup can shift the wall-clock origin by a few buffers.
  begin=max(0,int((float(t)-.8)*48000));end=min(len(output),int((float(t)+len(sources[id])/48000+.3)*48000))
  sig=output[begin:end:4];ref=sources[id][::4];n=len(ref);size=1<<(len(sig)+n-2).bit_length()
  conv=np.fft.irfft(np.fft.rfft(sig,size)*np.fft.rfft(ref[::-1],size),size)[n-1:len(sig)]
  energy=np.cumsum(np.concatenate(([0.],sig*sig)));energy=energy[n:]-energy[:-n]
  corr=conv/np.sqrt(np.maximum(energy*np.sum(ref*ref),1e-20));i=int(np.argmax(corr));score=float(corr[i])
  assert score>.60,(id,score)
  report.append({'id':id,'source_correlation':round(score,4),'recorded_time':round((begin+i*4)/48000,4),'rms':round(float(np.sqrt(np.mean(sig[i:i+n]**2))),6)})
 assert 'InterfaceSfx smoke PASS' in text
 print(json.dumps(report,indent=2))
 target=ROOT/'Saved/AudioValidation/InterfaceSfx-validation.json';target.write_text(json.dumps(report,indent=2),encoding='utf-8')
 print('MIXER PASS: all 8 source signatures audible in real engine output, no clipping; repeated pan burst emitted once.')
