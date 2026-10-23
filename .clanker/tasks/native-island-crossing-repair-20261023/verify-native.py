from pathlib import Path
import json,hashlib,re,math,subprocess
r=Path(__file__).resolve().parents[3];t=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();exe=sha(r/'var/build/stalthos-runtime/Release/Stalthos.exe');runs={};metrics={}
for f in t.glob('native-*.log'):
 m=re.search(r'evidence (.+)',f.read_text(encoding='utf-8-sig'))
 if not m:continue
 label=f.stem[7:];p=Path(m.group(1).strip());runs[label]=str(p);a=read(p/'runtime-receipt.json');w=read(p/'world-population.json')
 assert a['ok'] and a['support_failure_count']==0 and a['rendered_ground_support_fallbacks']==0,(label,a['support_failure_count'])
 assert read(p/'launch.json')['executable_sha256']==exe,label
 old={x['id']:x for x in w['snapshot_initial']['actors']};new={x['id']:x for x in w['snapshot_final']['actors']};assert set(old)==set(new) and len(new)==(8 if label.startswith('original-') else 12),label
 for identity,x in old.items():assert abs(new[identity]['age']-x['age']-(a['calendar_seconds']-a.get('loaded_calendar_seconds',0)))<1e-6,(label,identity)
 for resource in w['resource_ledger_final']['resources']:assert abs(sum(k['amount'] for k in w['resource_ledger_final']['sessions'] if k['resource_id']==resource['id'])-resource['consumed_quantity'])<1e-8,label
 assert not w['camera_drives_simulation'] and w['registry_owns_population'] and w['registry_owns_shared_ledger'],label
 metrics[label]={'path':str(p),'physical_seconds':a['physical_seconds'],'support_failures':a['support_failure_count'],'state_loaded':a['state_loaded'],'state_saved':a['state_saved']}
assert len(runs)==12,(len(runs),runs)
p=Path(runs['crossing']);j=read(p/'island-crossing.json');s=j['samples'];assert j['sea_entered'] and j['stone_landed'];assert all(c['saved'] for c in j['captures']) and len(j['captures'])==4
assert s[0]['physical_island']==1001 and s[-1]['physical_island']==1003 and s[-1]['landed'];swim=[x for x in s if x['water_mode']==2];assert len(swim)>10
assert all(x['physical_island']==0 and not x['resting'] and abs(x['body_y']+.55)<1e-5 and x['stance']=='water_buoyancy' for x in swim)
assert all(x['dry_support_failures']==0 and x['wade_support_failures']==0 for x in s)
assert all(x['stance']=='terrain_supported' for x in s if x['water_mode']!=2)
assert all(math.isfinite(x['body_y']) and x['actor_count']==12 for x in s)
assert max(math.hypot(x['x']-y['x'],x['z']-y['z']) for x,y in zip(s,s[1:]))<1.5,'pose discontinuity'
assert s[-1]['fatigue']>s[0]['fatigue'] and max(x['fatigue'] for x in swim)>.1
cam=read(Path(runs['camera'])/'island-crossing.json');c=cam['samples'];assert not cam['sea_entered'] and not cam['stone_landed'];assert all(x['physical_island']==1001 and x['npc_physical_active']==c[0]['npc_physical_active'] and x['ledger']==c[0]['ledger'] for x in c)
assert max(math.hypot(x['x']-c[0]['x'],x['z']-c[0]['z']) for x in c)<1e-6
assert max(math.hypot(x['camera_x']-c[0]['camera_x'],x['camera_z']-c[0]['camera_z']) for x in c)>5
long=read(Path(runs['long-start'])/'island-crossing.json')['samples'];assert math.hypot(long[0]['x']-s[0]['x'],long[0]['z']-s[0]['z'])<.05,'Longstride fixture overwritten by birth'
assert read(Path(runs['partial-save'])/'connected-saved-candidate.json')==read(Path(runs['partial-load'])/'connected-restored-before-first-tick.json')
for label in ('original-save','partial-save'):assert metrics[label]['state_saved']
for label in ('original-load','partial-load'):assert metrics[label]['state_loaded']
for name in ('connected-creature','connected-plant','connected-woodland'):
 pin=read(r/f'stalthos-runtime/config/{name}.json');entries=([(r/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(r/pin['path'],pin['sha256'])] if name=='connected-plant' else [(r/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
 for path,h in entries:assert sha(path)==h,str(path)
for path,h in read(t/'preservation-before.json').items():assert sha(r/path)==h,path
out={'ok':True,'executable_sha256':exe,'same_executable_successful_runs':len(runs),'runs':metrics,'crossing':{'physical_seconds_to_landing':next(x['physical_seconds'] for x in s if x['landed']),'swim_samples':len(swim),'start':[s[0]['x'],s[0]['z']],'end':[s[-1]['x'],s[-1]['z']],'final_fatigue':s[-1]['fatigue']},'exact_partial_restart':True,'pins_unchanged':True,'preserved_paths':253,'limits':['One Main-to-Stone crossing, not all-four traversal','No new-island population or foundation region migration','No full connected save or complete all-species family claim','No target30FPS or final swimming/scenery acceptance']};(t/'native-verification.json').write_text(json.dumps(out,indent=2)+'\n');print('ISLAND_NATIVE_VERIFIED')
