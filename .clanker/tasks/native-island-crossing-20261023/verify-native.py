from pathlib import Path
import json,hashlib,re,math
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-20261023';read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));runs={};exe=hashlib.sha256((r/'var/build/stalthos-runtime/Release/Stalthos.exe').read_bytes()).hexdigest()
for f in t.glob('native-*.log'):
 m=re.search(r'evidence (.+)',f.read_text(encoding='utf-8-sig'))
 if not m:continue
 p=Path(m.group(1).strip());a=read(p/'runtime-receipt.json');assert a['ok'];assert read(p/'launch.json')['executable_sha256']==exe
 runs[f.stem[7:]]={'path':str(p),'physical_seconds':a['physical_seconds'],'support_failures':a['support_failure_count'],'ground_fallbacks':a['rendered_ground_support_fallbacks'],'state_loaded':a['state_loaded'],'state_saved':a['state_saved']}
 if (p/'island-crossing.json').exists():
  j=read(p/'island-crossing.json');s=j['samples'];swim=[x for x in s if x['water_mode']==2];assert all(x['physical_island']==0 and not x['resting'] and abs(x['body_y']+.55)<1e-5 for x in swim)
  runs[f.stem[7:]].update({'sea_entered':j['sea_entered'],'stone_landed':j['stone_landed'],'sample_count':len(s),'swim_samples':len(swim),'last_pose':[s[-1]['x'],s[-1]['z']],'final_fatigue':s[-1]['fatigue'],'captures':j['captures']})
  if j['probe']==2:assert max(math.hypot(x['x']-s[0]['x'],x['z']-s[0]['z']) for x in s)<1e-6
if 'partial-save' in runs and 'partial-load' in runs:assert read(Path(runs['partial-save']['path'])/'connected-saved-candidate.json')==read(Path(runs['partial-load']['path'])/'connected-restored-before-first-tick.json')
(t/'native-verification.json').write_text(json.dumps({'executable_sha256':exe,'runs':runs,'native_crossing_accepted':False,'reason':'Pinned embodiment did not land in160s; terrain-foot support diagnostics report failures during water motion. Domain pass is not native acceptance.'},indent=2)+'\n')
print(json.dumps(runs,indent=2))
