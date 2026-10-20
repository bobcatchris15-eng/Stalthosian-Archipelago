import pathlib,re,json,wave,array,math,statistics,hashlib
root=pathlib.Path('.');task=root/'.clanker/tasks/body-audio-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def folder(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
def wav(path):
 with wave.open(str(path)) as w:
  assert w.getsampwidth()==2;rate=w.getframerate();channels=w.getnchannels();a=array.array('h',w.readframes(w.getnframes()))
 return rate,channels,a
logs={'motion':'native-motion-bounded.log','camera':'native-camera-bounded.log','death':'native-death-bounded.log','normal':'native-normal-bounded.log','original':'original-mode-bounded.log'};metrics={};exe=read(task/'build-receipt.json')['executable_sha256'];base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json');source_hashes=set()
for key,log in logs.items():
 p=folder(log);r=read(p/'runtime-receipt.json');assert r['ok'] and read(p/'launch.json')['executable_sha256']==exe
 metrics[key]={'path':str(p),'support_failures':r['support_failure_count'],'support_fallbacks':r['rendered_ground_support_fallbacks']}
 if key=='original':assert not (p/'body-audio.json').exists() and not (p/'body-rustle-source.wav').exists();continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 w=read(p/'world-population.json');assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 j=read(p/'body-audio.json');assert j['ready'] and not j['global_audio_settings_modified'] and j['event_capacity']==256;events=j['events'];assert j['events_dropped']==0 and j['requests_total']==len(events);source_hashes.add(hashlib.sha256((p/'body-rustle-source.wav').read_bytes()).hexdigest())
 if key=='death':
  assert events and all(v['hero_id']==2 and 12<=v['physical_seconds']<14 for v in events)
  d=read(p/'death-next-life.json');assert len(d['events'])==2 and d['events'][1]['old_death_known'] and d['events'][1]['npc_snapshot_unchanged_at_handoff'] and d['events'][1]['ledger_unchanged_at_handoff'];metrics[key]['dead_and_spawn_silence_new_life_motion']=True;continue
 if key=='normal':assert j['probe_mode']==0 and events and read(p/'hero-exploration.json')['enabled'];continue
 assert j['native_output_captured'] and j['capture_state']=='captured_own_process_tree' and j['process_capture_activation_hresult']==0
 rate,channels,a=wav(p/'native-output.wav');assert rate==44100 and channels==2 and a
 rms=math.sqrt(sum(v*v for v in a)/len(a))/32768;peak=max(abs(v) for v in a)/32768
 metrics[key].update(native_output_sha256=hashlib.sha256((p/'native-output.wav').read_bytes()).hexdigest(),rate=rate,channels=channels,captured_seconds=len(a)/rate/channels,rms=rms,peak=peak,events=len(events),voice_samples_played=j['max_voice_samples_played'],capture_mode=j['capture_state'])
 if key=='camera':
  assert not events and j['max_voice_samples_played']==0 and rms<.00005 and peak<.0001
  s=read(p/'hero-rest.json')['samples'];assert len({(v['body_x'],v['body_z']) for v in s})==1
  c=read(p/'hero-autonomy.json')['samples'];assert math.hypot(max(v['camera_target_x'] for v in c)-min(v['camera_target_x'] for v in c),max(v['camera_target_z'] for v in c)-min(v['camera_target_z'] for v in c))>5;continue
 assert events and j['max_voice_samples_played']>0 and rms>.001 and peak>.01 and peak<.1
 assert all(2<=v['physical_seconds']<6 or 8<=v['physical_seconds']<10 for v in events)
 hop=int(rate*.01)*channels;active=[]
 for i in range(0,len(a),hop):
  block=a[i:i+hop];active.append(math.sqrt(sum(v*v for v in block)/len(block))/32768>.0002)
 segments=[];start=None
 for i,on in enumerate(active+[False]):
  if on and start is None:start=i*.01
  if not on and start is not None:segments.append((start,i*.01));start=None
 assert len(segments)==len(events) and len(events)>=10 and all(.1<=end-start<=.16 for start,end in segments)
 intervals=[b[0]-a[0] for a,b in zip(segments,segments[1:]) if b[0]-a[0]<1];assert abs(statistics.median(intervals)-.45)<.02
 # Long stationary portions contain only the PCM conversion dither floor.
 for lo,hi in [(0,1.5),(6,7.8),(10.2,12)]:
  block=a[int(lo*rate)*channels:int(hi*rate)*channels];assert math.sqrt(sum(v*v for v in block)/len(block))/32768<.00005
 metrics[key].update(bursts=segments,median_burst_interval=statistics.median(intervals),stationary_dither_only=True)
assert len(source_hashes)==1
prior=read(task/'preserved-untracked.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['source_wave_sha256']=next(iter(source_hashes));metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior);metrics['review']='root-only; preserved independent reviewers usage-limited';metrics['scope']='actual own-process native digital mix captured, not physical speaker/human or acoustic/foot-contact acceptance; capture startup/frame stalls prevent sample-exact latency/continuous-wall-clock claims'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('BODY_AUDIO_NATIVE_VERIFIED actual own-process output matched movement bursts, camera/stationary dither-only, dead/spawn refusal and fresh-life motion, normal baseline and preservation')
