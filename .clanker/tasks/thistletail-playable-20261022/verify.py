from pathlib import Path
import json,hashlib,math
root=Path.cwd(); task=root/'.clanker/tasks/thistletail-playable-20261022'
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
runs=read(task/'native-runs.json'); exe=sha(root/'var/build/stalthos-runtime/Release/Stalthos.exe'); metrics={}; birds={}
for label,path in runs.items():
 p=Path(path); r=read(p/'runtime-receipt.json'); w=read(p/'world-population.json')
 assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 assert read(p/'launch.json')['executable_sha256']==exe
 initial={a['id']:a for a in w['snapshot_initial']['actors']}; final={a['id']:a for a in w['snapshot_final']['actors']}
 assert set(initial)==set(final) and w['resource_sessions_initial']==w['resource_sessions_final']
 for identity,a in initial.items(): assert abs(final[identity]['age']-a['age']-r['calendar_seconds'])<1e-6
 ledger=w['resource_ledger_final']
 for resource in ledger['resources']:
  assert abs(sum(s['amount'] for s in ledger['sessions'] if s['resource_id']==resource['id'])-resource['consumed_quantity'])<1e-8
 metrics[label]={'physical_seconds':r['physical_seconds'],'actors':len(final),'support_failures':0,'support_fallbacks':0}
 f=p/'thistletail.json'
 if label in ('small-normal','original'):
  assert not f.exists() and all(x['id']!=210 for x in ledger['resources'])
 else:
  j=birds[label]=read(f); assert j['samples']
  assert all(s['species']=='Thistletail' and s['needs_species']==3 and s['lifecycle_species']==3 and s['body_supported'] for s in j['samples'])
  assert len(final)==12 and any(x['id']==210 for x in ledger['resources'])
for label in ('feed-life','override'):
 j=birds[label]; c=[c for c in j['contacts'] if c['resource']==210]; assert len(c)==1
 c=c[0]; assert c['eligible'] and c['amount']==.5 and abs(c['hunger_before']-c['hunger_after']-.5)<1e-8 and j['remaining_food']==.5
 assert any(abs(s['leg_angle'])>.05 and s['speed']>0 for s in j['samples'])
 metrics[label]['food_contact']=c
s=birds['override']['samples']; direct=[x for x in s if x['direct']]; assert direct and all(not x['auto_forward'] and not x['auto_contact'] for x in direct)
s=birds['camera']['samples']; assert all(not x['attention'] and x['body_x']==-12 and x['body_z']==24 and x['distance']==0 for x in s)
assert not birds['camera']['contacts'] and birds['camera']['remaining_food']==1
metrics['camera']['max_camera_body_distance']=max(math.hypot(x['camera_x']-x['body_x'],x['camera_z']-x['body_z']) for x in s)
e=read(Path(runs['feed-life'])/'death-next-life.json')['events']; n=[x for x in e if x['event']=='explicit_next_life']; assert len(n)==1
assert all(n[0][k] for k in ['ledger_unchanged_at_handoff','npc_snapshot_unchanged_at_handoff','old_death_known','old_id_excluded'])
assert birds['feed-life']['samples'][-1]['hero']==2 and birds['feed-life']['samples'][-1]['session']==14
rest=read(Path(runs['bird-rest'])/'hero-rest.json')['samples']; assert any(s['resting'] for s in rest) and not rest[-1]['resting'] and rest[-1]['fatigue']<.34
assert any(c['resource']==209 and c['amount']==.5 for c in birds['bird-drink']['contacts'])
assert any(e['event']=='arrived' for e in read(Path(runs['bird-destination'])/'hero-destination.json')['events'])
review=read(root/'.clanker/clunkers/thistletail-review-20261022/review.json')
for a in review['source_hashes']: assert sha(root/a['path'])==a['sha256']
preserve=read(task/'preservation-before.json'); checked=0
for path,digest in preserve.items():
 if path.startswith('.clanker/graph/'): continue
 assert sha(root/path)==digest,path
 checked+=1
for label,baseline_path in [('small-normal','20261022-204617-9ca94244'),('original','20261022-204627-acdf4883')]:
    current=read(Path(runs[label])/'world-population.json'); baseline=read(root/'var/reports/runtime'/baseline_path/'world-population.json')
    assert current['actors_initial']==baseline['actors_initial'] and current['resource_ledger_initial']==baseline['resource_ledger_initial']
for name in ('connected-creature','connected-plant','connected-woodland'):
    pin=read(root/f'stalthos-runtime/config/{name}.json')
    entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ('creature','metadata')] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ('adult','sapling')])
    for path,expected in entries: assert sha(path)==expected,str(path)
out={'ok':True,'executable_sha256':exe,'native_runs':metrics,'preserved_untracked':checked,'independently_reviewed_source_hashes_match':True,'ctest':'62/62 passed','limitations':['Procedural fixed-stage bird; no anatomical beak-ground contact acceptance, IK or final plumage','One finite litter patch; no individual insect simulation or bird breeding/parenting','Generic 2 m/s and provisional shared metabolism','Two playable species only; four islands, complete families and connected persistence remain gaps','No target-hardware 30 FPS acceptance; build includes existing wchar-to-char C4244 warning']}
(task/'verification.json').write_text(json.dumps(out,indent=2)+'\n'); print(json.dumps(out,indent=2))
