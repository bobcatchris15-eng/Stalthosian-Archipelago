import pathlib,json,re,hashlib,math
root=pathlib.Path('.');task=root/'.clanker/tasks/hero-exploration-20261020'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def folder(log):return pathlib.Path(re.search(r'evidence ([^\r\n]+)',(task/log).read_text(encoding='utf-8-sig'))[1])
logs={'proxy':'native-proxy-final.log','animal':'native-animal.log','override':'native-override.log','camera':'native-camera.log','normal':'native-normal.log','death':'native-death.log','original':'original-mode.log'};metrics={};firstTargets={};firstStops={};exe=read(task/'build-receipt.json')['executable_sha256'];base=read(root/'var/reports/runtime/20261020-113545-5ed05743/world-population.json')
for key,log in logs.items():
 p=folder(log);r=read(p/'runtime-receipt.json');assert r['ok'] and read(p/'launch.json')['executable_sha256']==exe
 metrics[key]={'path':str(p),'support_failures':r['support_failure_count'],'support_fallbacks':r['rendered_ground_support_fallbacks']}
 if key=='original':assert not (p/'hero-exploration.json').exists();continue
 assert r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 w=read(p/'world-population.json');assert w['actors_initial']==base['actors_initial'] and w['resource_ledger_initial']==base['resource_ledger_initial'] and w['resource_sessions_initial']==base['resource_sessions_initial']
 j=read(p/'hero-exploration.json')
 if key=='death':
  assert not j['enabled'] and not j['samples'];d=read(p/'death-next-life.json');assert len(d['events'])==2 and d['events'][1]['old_death_known'] and d['events'][1]['npc_snapshot_unchanged_at_handoff'] and d['events'][1]['ledger_unchanged_at_handoff'];continue
 assert j['enabled'];s=j['samples'];assert s and all(math.isfinite(v['body_x']) and math.isfinite(v['body_z']) for v in s)
 assert all(not v['destination'] or v['phase']==0 for v in s)
 assert all(v['phase']==0 for v in s if v['direct'] or v['resting'])
 if key=='override':
  assert len(j['events'])==2 and j['events'][1]['accepted'];assert any(v['direct'] and v['speed']<0 for v in s)
  contacts=read(p/'hero-autonomy.json')['contacts'];assert len(contacts)==1 and contacts[0]['resource']==209 and contacts[0]['eligible'] and contacts[0]['amount']==.5 and abs(contacts[0]['thirst_before']-contacts[0]['thirst_after']-.5)<1e-9
  assert all(v['phase']==0 for v in s if 10.05<v['physical_seconds']<12.8)
  assert any(v['phase']==1 and 8<v['physical_seconds']<10 for v in s) and any(v['phase']==1 and 13.1<v['physical_seconds']<15 for v in s)
  assert any(v['destination'] and v['goal_contextual'] and v['phase']==0 for v in s)
  metrics[key].update(direct_override=True,higher_need_drink_relief=.5,contextual_override=True);continue
 moving=[v for v in s if v['phase']==1];assert moving and all(not v['goal_contextual'] and not v['direct'] and not v['destination'] for v in moving)
 assert all(abs(v['thirst']-.0002*v['physical_seconds'])<1e-8 for v in s)
 assert any(v['speed']>0 for v in moving)
 if key=='normal':assert not j['synthetic_start_input_need_destination_noon'] and not j['events'];continue
 assert j['synthetic_start_input_need_destination_noon'] and len(j['captures'])>=2 and all(c['saved'] for c in j['captures'])
 looking=[v for v in s if v['phase']==2];assert looking and all(v['speed']==0 and v['cue_visible'] and not v['resting'] for v in looking)
 firstTargets[key]=(moving[0]['target_x'],moving[0]['target_z']);firstStops[key]=(looking[0]['body_x'],looking[0]['body_z']);assert abs(math.hypot(looking[0]['body_x']-132,looking[0]['body_z']-30)-1.25)<.08
 firstLook=looking[0]['physical_seconds'];dwell=[v for v in looking if v['physical_seconds']<firstLook+2.7];assert len(dwell)>15 and len({(v['body_x'],v['body_z']) for v in dwell})==1
 metrics[key].update(first_target=firstTargets[key],first_arrival=firstStops[key],pause_seconds_observed=dwell[-1]['physical_seconds']-dwell[0]['physical_seconds'],captures=len(j['captures']))
 if key=='camera':assert min(math.hypot(v['camera_x']-v['body_x'],v['camera_z']-v['body_z']) for v in s)>18
assert firstTargets['animal']==firstTargets['camera'];assert math.dist(firstStops['animal'],firstStops['camera'])<.02
for key in ['normal','animal','camera','proxy']:
 p=folder(logs[key]);stage=read(p/'life-stage.json');assert stage['initial_age']==0 and all(abs(v['age']-v['calendar_seconds'])<1e-8 for v in stage['samples'])
prior=read(task/'preserved-untracked.json')
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 assert (root/path).is_file() and hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
for name in ['connected-creature','connected-plant','connected-woodland']:
 pin=read(root/f'stalthos-runtime/config/{name}.json');entries=([(root/pin['asset_root']/pin[k],pin[k+'_sha256']) for k in ['creature','metadata']] if name=='connected-creature' else [(root/pin['path'],pin['sha256'])] if name=='connected-plant' else [(root/pin[k]['path'],pin[k]['sha256']) for k in ['adult','sapling']])
 for path,digest in entries:assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
metrics['preserved_original_untracked']=len(prior)-('.clanker/graph/built-at-rev' in prior);metrics['review']='root-only; both preserved independent reviewers usage-limited';metrics['camera_scope']='native detached initial anchor20m away; identical first target and arrival, no camera knowledge; no moving-camera or human input acceptance claim'
(task/'verification.json').write_text(json.dumps(metrics,indent=2));print('EXPLORATION_NATIVE_VERIFIED supported move/pause, immediate direct/need/context overrides, detached body-local target, ordinary mode and prior death/original baselines preserved')
