from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exe=sha(root/'var/build/stalthos-runtime/Release/Stalthos.exe');metrics={}
for label,path in read(task/'native-runs.json').items():
 p=Path(path);r=read(p/'runtime-receipt.json');w=read(p/'world-population.json')
 assert r['ok'] and r['rendered_ground_support_fallbacks']==0,label
 assert r['support_failure_count']==0,label
 assert read(p/'launch.json')['executable_sha256']==exe,label
 initial={a['id']:a for a in w['snapshot_initial']['actors']};final={a['id']:a for a in w['snapshot_final']['actors']}
 if label=='hatch':
  assert set(initial)<set(final) and len(final)==len(initial)+1,label
  old_bindings=dict(w['resource_sessions_initial']);new_bindings=dict(w['resource_sessions_final']);assert all(new_bindings[k]==v for k,v in old_bindings.items()),label
 else:assert set(initial)==set(final) and w['resource_sessions_initial']==w['resource_sessions_final'],label
 for identity,a in initial.items():assert abs(final[identity]['age']-a['age']-r['calendar_seconds'])<1e-6,(label,identity)
 ledger=w['resource_ledger_final']
 for resource in ledger['resources']:
  assert abs(sum(s['amount'] for s in ledger['sessions'] if s['resource_id']==resource['id'])-resource['consumed_quantity'])<1e-8,label
 metrics[label]={'path':path,'physical_seconds':r['physical_seconds'],'actors':len(final),'yaw_only_pose_frames':r['support_failure_count'],'rendered_ground_fallbacks':r['rendered_ground_support_fallbacks']}
for label,baseline_path in [('small-normal','20261022-204617-9ca94244'),('original','20261022-204627-acdf4883')]:
 current=read(Path(read(task/'native-runs.json')[label])/'world-population.json');baseline=read(root/'var/reports/runtime'/baseline_path/'world-population.json')
 assert current['actors_initial']==baseline['actors_initial'] and current['resource_ledger_initial']==baseline['resource_ledger_initial']
for path,digest in read(task/'preservation-before.json').items():assert sha(root/path)==digest,path
for name in ('connected-creature','connected-plant','connected-woodland'):
 pin=read(root/f'stalthos-runtime/config/{name}.json')
 entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
 for path,expected in entries:assert sha(path)==expected,str(path)
output={'ok':True,'executable_sha256':exe,'native_runs':metrics,'unrelated_untracked_preserved':len(read(task/'preservation-before.json')),'pinned_asset_hashes_unchanged':True}
(task/'common-verification.json').write_text(json.dumps(output,indent=2)+'\n')
print(json.dumps(output,indent=2))
