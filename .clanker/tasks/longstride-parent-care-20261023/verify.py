from pathlib import Path
import json,re,math,hashlib,subprocess
r=Path(__file__).resolve().parents[3];t=Path(__file__).parent;read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();runs={}
for f in t.glob('native-*.log'):
 if f.stem=='native-matrix-output':continue
 m=re.search(r'evidence (.+)',f.read_text(encoding='utf-8-sig'))
 if m:runs[f.stem[7:]]=m.group(1).strip()
assert len(runs)==12,(len(runs),runs)
exe=sha(r/'var/build/stalthos-runtime/Release/Stalthos.exe');metrics={}
for label,path in runs.items():
 p=Path(path);a=read(p/'runtime-receipt.json');w=read(p/'world-population.json');assert a['ok'] and a['support_failure_count']==0 and a['rendered_ground_support_fallbacks']==0,label;assert read(p/'launch.json')['executable_sha256']==exe,label
 old={x['id']:x for x in w['snapshot_initial']['actors']};new={x['id']:x for x in w['snapshot_final']['actors']}
 assert set(old)<=set(new) and len(new)==len(old)+(label=='small-hatch'),label
 for identity,x in old.items():assert abs(new[identity]['age']-x['age']-(a['calendar_seconds']-a.get('loaded_calendar_seconds',0)))<1e-6,(label,identity)
 for x in w['resource_ledger_final']['resources']:assert abs(sum(k['amount'] for k in w['resource_ledger_final']['sessions'] if k['resource_id']==x['id'])-x['consumed_quantity'])<1e-8,label
 metrics[label]={'path':path,'physical_seconds':a['physical_seconds'],'actors':len(new)}
a=read(Path(runs['partial-save'])/'connected-saved-candidate.json');b=read(Path(runs['partial-load'])/'connected-restored-before-first-tick.json');assert a==b
for label in ['care','normal']:
 j=read(Path(runs[label])/'longstride-care.json');birth=[e for e in j['events'] if e['event']=='young_hero_birth_near_actual_wild_caregiver'];assert len(birth)==1 and not birth[0]['npc_position_modified'];s=[x for x in j['samples'] if x['parent']];assert s and all(x['parent']['id']==birth[0]['caregiver'] and x['body_supported'] for x in s)
 assert any(x['following_goal'] for x in s) and any('Waiting near' in x['status'] for x in s),label
 if label=='care':
  direct=[x for x in s if 5.1<x['physical_seconds']<6.9];assert direct and all(x['direct'] and not x['following_goal'] for x in direct)
  distance=lambda x:math.hypot(x['body_x']-x['parent']['x'],x['body_z']-x['parent']['z'])
  peak=max(distance(x) for x in s if 5<x['physical_seconds']<8);returned=min(distance(x) for x in s if 8<x['physical_seconds']<18);assert peak-returned>.3,(peak,returned)
  later=[x for x in j['samples'] if x['hero']==2];assert later and all(x['young']==0 and x['caregiver']==0 and not x['following_goal'] for x in later)
  n=[e for e in read(Path(runs[label])/'death-next-life.json')['events'] if e['event']=='explicit_next_life'];assert len(n)==1 and n[0]['npc_snapshot_unchanged_at_handoff'] and n[0]['ledger_unchanged_at_handoff'];assert all(x['saved'] for x in j['captures'])
  follow={'peak_separation':peak,'returned_separation':returned,'caregiver':birth[0]['caregiver'],'parent_native_travel':max(x['parent']['distance'] for x in s)-min(x['parent']['distance'] for x in s),'parent_thirst_start':s[0]['parent']['thirst'],'parent_thirst_end':s[-1]['parent']['thirst']}
j=read(Path(runs['camera'])/'longstride-care.json');s=j['samples'];assert s and all(not x['present'] and not x['following_goal'] for x in s);assert max(math.hypot(x['body_x']-s[0]['body_x'],x['body_z']-s[0]['body_z']) for x in s)<1e-6
camera_parent_min=min(math.hypot(x['camera_x']-x['parent']['x'],x['camera_z']-x['parent']['z']) for x in s if x['parent'])
for path,digest in read(t/'preservation-before.json').items():assert sha(r/path)==digest,path
for e in read(r/'.clanker/clunkers/longstride-parent-care-review-20261023/source-hashes.json'):
 if e['path']=='stalthos-runtime/engine/main.cpp':continue
 assert hashlib.sha256((r/e['path']).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==e['canonical_lf_sha256'];assert subprocess.check_output(['git','hash-object','--path='+e['path'],e['path']],cwd=r).decode().strip()==e['git_blob']
base=subprocess.check_output(['git','show','e2131ce:stalthos-runtime/engine/main.cpp'],cwd=r).decode().replace('\r\n','\n');expected=base.replace('const bool directInput=((carnivoreProbe==1 || carnivoreProbe==2) && userReverse)','const bool directInput=((parentCareProbe || carnivoreProbe==1 || carnivoreProbe==2) && userReverse)').replace('{"following_goal",careGoalOwned},{"status",careDecision.status}','{"following_goal",careGoalOwned},{"direct",directInput},{"status",careDecision.status}');assert (r/'stalthos-runtime/engine/main.cpp').read_text(encoding='utf-8')==expected
out={'ok':True,'ctest':'68/68','same_executable_native_runs':len(runs),'executable_sha256':exe,'native_runs':metrics,'care':follow,'camera_parent_nearest_meters':camera_parent_min,'preserved_unrelated_paths':len(read(t/'preservation-before.json')),'source_review':'e2131ce independently reviewed9paths; only two-line exact reconstructed diagnostic authority/sample delta c41510c applied by root, ordinary behavior unchanged','limits':['Actual parent remains drinking in native window; moving journey/family provisioning not native proven','Camera remains constrained by tether and cannot reach actual remote parent; only absence of camera-granted follow intent proven','Caregiver death domain only; young death/N native, no new caregiver afterN','Generic NPC art, full allspecies families, full connected saves, four islands and target30FPS remain unaccepted']}
(t/'native-runs.json').write_text(json.dumps(runs,indent=2)+'\n');(t/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print('LONGSTRIDE_CARE_BOUNDED_NATIVE_VERIFIED')



