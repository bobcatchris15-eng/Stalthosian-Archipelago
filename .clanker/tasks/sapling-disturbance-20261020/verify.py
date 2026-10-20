import json,pathlib,re,hashlib,math
root=pathlib.Path('.');task=root/'.clanker/tasks/sapling-disturbance-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def folder(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
logs={'proxy':'native-proxy.log','animal':'native-animal.log','camera':'native-camera.log','normal':'native-normal.log','death':'native-death.log','original':'original-mode.log'};metrics={};exe=read(task/'build-receipt.json')['executable_sha256'];base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json')
for key,log in logs.items():
 p=folder(log);r=read(p/'runtime-receipt.json');assert r['ok'] and read(p/'launch.json')['executable_sha256']==exe
 metrics[key]={'path':str(p),'support_failures':r['support_failure_count'],'support_fallbacks':r['rendered_ground_support_fallbacks']}
 if key=='original':assert not (p/'sapling-disturbance.json').exists();continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 w=read(p/'world-population.json');assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 assert {a['id'] for a in w['actors_initial']}=={a['id'] for a in w['actors_final']}
 before={(k['session_id'],k['resource_id']):k['amount'] for k in w['resource_ledger_initial']['sessions']};after={(k['session_id'],k['resource_id']):k['amount'] for k in w['resource_ledger_final']['sessions']};assert all(after.get(k,-1)>=v for k,v in before.items())
 j=read(p/'sapling-disturbance.json');s=j['samples'];assert j['available'] and j['tree_id']==9006 and j['region']==1001 and s
 first={t['id']:t for t in s[0]['trees']};assert len(first)==10
 assert all(abs(v['physical_seconds']-v['calendar_seconds'])<1e-8 for v in s)
 for v in s:
  for t in v['trees']:
   initial=first[t['id']];assert [q['mesh'] for q in t['parts']]==[q['mesh'] for q in initial['parts']]
   if t['id']!=9006 or not v['visual_applied']:assert t==initial,'unexpected neighboring/root/prototype-instance change'
   else:
    assert t['rotation']!=initial['rotation'];assert all(abs(q['color'][0]-1)<1e-6 and abs(q['color'][1]-.35)<1e-6 and abs(q['color'][2]-.12)<1e-6 for q in t['parts'])
 if key in ['normal','death']:
  assert not j['events'] and all(not v['damaged'] and v['revision']==0 for v in s)
  if key=='normal':assert not j['synthetic_start_input_noon'] and read(p/'hero-exploration.json')['enabled']
  else:
   d=read(p/'death-next-life.json');assert len(d['events'])==2 and d['events'][1]['old_death_known'] and d['events'][1]['npc_snapshot_unchanged_at_handoff'] and d['events'][1]['ledger_unchanged_at_handoff']
  continue
 assert j['synthetic_start_input_noon'] and len(j['events'])==2 and len(j['captures'])==4 and all(c['saved'] for c in j['captures'])
 if key=='camera':
  assert all(not e['applied'] and not e['eligible'] for e in j['events'])
  assert all(not v['eligible'] and not v['damaged'] and not v['cue_visible'] and v['revision']==0 and v['body_x']==126.5 and abs(v['camera_x']-146.5)<1e-4 for v in s)
  metrics[key]['camera_only_rejected']=True;continue
 applied,repeated=j['events'];assert applied['applied'] and applied['eligible'] and not repeated['applied'] and repeated['eligible'] and applied['revision']==repeated['revision']==1
 assert all(v['revision']==1 and v['changed_at']==applied['calendar_seconds'] and v['visual_applied'] for v in s if v['physical_seconds']>=applied['physical_seconds'])
 away=[v for v in s if v['damaged'] and not v['eligible']];returned=[v for v in s if v['physical_seconds']>repeated['physical_seconds'] and v['eligible'] and v['damaged']];assert away and returned
 assert max(abs(v['body_x']-146.5) for v in s)>(5.5 if key=='proxy' else 2.2)
 assert abs(returned[0]['body_x']-146.5)<.03
 # All saplings share geometry IDs, while their object instances and changed colors remain independent.
 assert {q['mesh'] for q in first[9006]['parts']}=={q['mesh'] for q in first[9007]['parts']}
 metrics[key].update(exception_revision=1,changed_at=applied['calendar_seconds'],repeated_action_idempotent=True,departure_m=max(abs(v['body_x']-146.5) for v in s),return_error_m=abs(returned[0]['body_x']-146.5),neighbor_instances_unchanged=9,shared_mesh_ids_preserved=True,captures=4)
prior=read(task/'preserved-untracked.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior);metrics['review']='root-only; two preserved independent reviewers usage-limited';metrics['scope']='one deliberate proximity action and session-world StandingDamaged exception; root lean/per-object tint, no impact/contact animation, tree mechanics or connected save acceptance'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('SAPLING_NATIVE_VERIFIED real exception, living supported body action, visible isolated instance, departure/return, repeat idempotence, camera rejection and prior baselines preserved')
