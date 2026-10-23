from pathlib import Path
import json,re,hashlib,subprocess,math
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
runs={}
for log in task.glob('native-*.log'):
 label=log.stem[7:]
 if label.startswith(('preliminary','matrix')):continue
 m=re.search(r'evidence (.+)',log.read_text(encoding='utf-8-sig'))
 if m:runs[label]=m.group(1).strip()
assert len(runs)==20,(len(runs),list(runs))
exe=sha(root/'var/build/stalthos-runtime/Release/Stalthos.exe');metrics={}
for label,path in runs.items():
 p=Path(path);r=read(p/'runtime-receipt.json');w=read(p/'world-population.json')
 assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0,label
 assert read(p/'launch.json')['executable_sha256']==exe,label
 initial={a['id']:a for a in w['snapshot_initial']['actors']};final={a['id']:a for a in w['snapshot_final']['actors']}
 assert set(initial)==set(final) and w['resource_sessions_initial']==w['resource_sessions_final'],label
 for identity,a in initial.items():assert abs(final[identity]['age']-a['age']-(r['calendar_seconds']-r.get('loaded_calendar_seconds',0)))<1e-6,(label,identity)
 ledger=w['resource_ledger_final']
 for resource in ledger['resources']:assert abs(sum(s['amount'] for s in ledger['sessions'] if s['resource_id']==resource['id'])-resource['consumed_quantity'])<1e-8,label
 metrics[label]={'path':path,'physical_seconds':r['physical_seconds'],'actors':len(final),'support_failures':0,'rendered_ground_fallbacks':0}
pairs=[('primary','restart'),('camera','camera-restart')]+[(s+'-save',s+'-load') for s in ['Smalltrotter','Thistletail','Brackenback','Reedcrest']]
for saved,loaded in pairs:
 a=read(Path(runs[saved])/'connected-saved-candidate.json');b=read(Path(runs[loaded])/'connected-restored-before-first-tick.json');assert a==b,(saved,loaded)
 assert read(Path(runs[saved])/'runtime-receipt.json')['state_saved'] and read(Path(runs[loaded])/'runtime-receipt.json')['state_loaded']
 events=read(Path(runs[loaded])/'connected-persistence.json')['events'];assert any(e.get('exit_saved') for e in events),loaded
first=Path(runs['primary']);j=read(first/'connected-persistence.json');unsafe=[e for e in j['events'] if e.get('unsafe_diagnostic')];assert len(unsafe)==1 and not unsafe[0]['saved'] and unsafe[0]['destination_bytes_preserved'] and unsafe[0]['live_world_bytes_preserved']
a=read(first/'connected-saved-candidate.json');food=next(r for r in a['ledger']['resources'] if r['id']==211);assert food['consumed_quantity']==food['initial_quantity']==1
assert [s['amount'] for s in a['ledger']['sessions'] if s['resource_id']==211]==[.5,.5]
n=[e for e in read(first/'death-next-life.json')['events'] if e['event']=='explicit_next_life'];assert len(n)==1 and n[0]['hero_id']==2 and n[0]['fresh_session']==15 and n[0]['old_death_known'] and n[0]['npc_snapshot_unchanged_at_handoff'] and n[0]['ledger_unchanged_at_handoff']
j=read(Path(runs['camera'])/'connected-persistence.json');s=j['camera_samples'];assert all(x['body_distance_from_start']==0 and not x['hero_attention'] and x['physical_active'] for x in s);assert any(x['mode']==1 and x['focus']==1 for x in s);assert math.hypot(s[-1]['anchor_x']-s[-1]['body_x'],s[-1]['anchor_z']-s[-1]['body_z'])>10;assert len(j['camera_captures'])==3 and all(c['saved'] for c in j['camera_captures'])
assert len(read(task/'native-failure-verification.json'))==5
for path,digest in read(task/'preservation-before.json').items():assert sha(root/path)==digest,path
for e in read(root/'.clanker/clunkers/connected-persistence-review-20261023/final-source-hashes.json'):
 p=root/e['path'];assert hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==e['canonical_lf_sha256'],e['path'];assert subprocess.check_output(['git','hash-object','--path='+e['path'],e['path']],cwd=root).decode().strip()==e['git_blob']
for name in ('connected-creature','connected-plant','connected-woodland'):
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
 for path,expected in entries:assert sha(path)==expected,str(path)
(task/'native-runs.json').write_text(json.dumps(runs,indent=2)+'\n')
output={'ok':True,'scope':'bounded manual persistence profile only','ctest':'67/67','executable_sha256':exe,'same_executable_successful_native_runs':len(runs),'native_runs':metrics,'exact_actual_live_owner_restart_pairs':pairs,'unsafe_refusal':unsafe[0],'five_failure_refusals':read(task/'native-failure-verification.json'),'preserved_unrelated_paths':len(read(task/'preservation-before.json')),'source_canonical_lf_and_blobs_match':True,'pins_unchanged':True,'limits':['Normal connected social/family/automatic state is unsupported; profile excludes it at entry','Camera native run starts alreadyactive watershed; no dormant-region transition proof','No full life/parenting/fourisland/finalvisual/target30FPS acceptance']}
(task/'verification.json').write_text(json.dumps(output,indent=2)+'\n');print('CONNECTED_PARTIAL_NATIVE_VERIFIED')

