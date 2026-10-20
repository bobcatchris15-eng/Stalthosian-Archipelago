import json,pathlib,re,hashlib,math
root=pathlib.Path('.');task=root/'.clanker/tasks/death-next-life-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def evidence(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
metrics={};exe=read(task/'build-receipt.json')['executable_sha256'];base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json')
for key,log in [('proxy','native-proxy-final.log'),('animal','native-animal.log'),('normal','native-normal.log'),('original','original-mode.log')]:
 p=evidence(log);r=read(p/'runtime-receipt.json');assert r['ok'] and read(p/'launch.json')['executable_sha256']==exe
 metrics[key]={'path':str(p),'support_failures':r['support_failure_count'],'support_fallbacks':r['rendered_ground_support_fallbacks']}
 if key=='original':assert not (p/'death-next-life.json').exists();continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 w=read(p/'world-population.json');j=read(p/'death-next-life.json');s=j['samples'];assert s
 assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 initial_ids={a['id'] for a in s[0]['actors']};assert len(initial_ids)==12 and all({a['id'] for a in v['actors']}==initial_ids for v in s)
 assert all(abs(v['physical_seconds']-v['calendar_seconds'])<1e-8 for v in s)
 assert all(b['calendar_seconds']>a['calendar_seconds'] for a,b in zip(s,s[1:]))
 if key=='normal':assert not j['events'] and not j['synthetic_age_thirst_input_noon'] and all(v['hero_id']==1 and v['alive'] for v in s);continue
 assert j['synthetic_age_thirst_input_noon'] and len(j['events'])==2
 death,new=j['events'];assert death['event']=='death' and 6<=death['physical_seconds']<6.11 and new['event']=='explicit_next_life' and 10<=new['physical_seconds']<10.11
 assert new['hero_id']!=death['hero_id'] and new['hero_id'] not in initial_ids and new['old_death_known'] and new['old_id_excluded']
 assert new['ledger_unchanged_at_handoff'] and new['npc_snapshot_unchanged_at_handoff'] and new['npc_count']==12 and new['new_age']==0 and new['new_stage']=='hatchling'
 dead=[v for v in s if not v['alive']];assert len(dead)>5 and all(v['hero_id']==death['hero_id'] and v['old_death_known'] and v['speed']==0 for v in dead)
 assert len({(v['body_x'],v['body_z']) for v in dead})==1 and any(v['direct'] for v in dead)
 assert max(v['thirst'] for v in dead)-min(v['thirst'] for v in dead)<1e-10
 assert any(b['distance']>a['distance']+0.1 for a,b in zip(dead[0]['actors'],dead[-1]['actors']))
 alive=[v for v in s if v['hero_id']==new['hero_id']];assert alive and all(v['alive'] and v['old_death_known'] for v in alive)
 active=[v for v in alive if v['direct']];assert active and all(v['speed']!=0 for v in active)
 moved=math.hypot(alive[-1]['body_x']-alive[0]['body_x'],alive[-1]['body_z']-alive[0]['body_z']);assert moved>(3.8 if key=='proxy' else .28)
 history={};consumed={}
 for v in s:
  current={(k['session_id'],k['resource_id']):k['amount'] for k in v['ledger']['sessions']};assert all(current.get(k,-1)>=amount for k,amount in history.items());history=current
  for resource in v['ledger']['resources']:
   assert resource['consumed_quantity']>=consumed.get(resource['id'],0);consumed[resource['id']]=resource['consumed_quantity']
 npc_sessions={pair[1] for pair in w['resource_sessions_initial']};assert new['fresh_session'] not in npc_sessions
 old_contacts=[k for k in dead[-1]['ledger']['sessions'] if k['session_id'] not in npc_sessions];new_contacts=[k for k in alive[-1]['ledger']['sessions'] if k['session_id'] not in npc_sessions and k not in old_contacts]
 assert old_contacts and new_contacts and all(k['resource_id']==209 for k in old_contacts+new_contacts)
 contacts=read(p/'hero-autonomy.json')['contacts'];hero_contacts=[c for c in contacts if c['session'] in {k['session_id'] for k in old_contacts+new_contacts}];assert len(hero_contacts)==2 and all(c['eligible'] and c['amount']==.5 and abs(c['thirst_before']-c['thirst_after']-.5)<1e-9 for c in hero_contacts)
 assert {k['session_id'] for k in old_contacts}.isdisjoint(k['session_id'] for k in new_contacts)
 assert all(v['saved'] for v in j['captures']) and len(j['captures'])==4
 metrics[key].update(death_seconds=death['physical_seconds'],next_life_seconds=new['physical_seconds'],old_hero=death['hero_id'],new_hero=new['hero_id'],dead_input_refused=True,world_npcs_continued=True,history_preserved=True,old_contacts=old_contacts,new_contacts=new_contacts,new_direct_distance_m=moved)
prior=read(task/'preserved-untracked.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json')
 entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior)
metrics['review']='root-only; both preserved independent reviewers usage-limited'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('DEATH_NEXT_LIFE_NATIVE_VERIFIED actual death, refused input, continuing NPCs/calendar, explicit fresh identity/session, retained finite history and live controls/contact; ordinary baseline preserved')
