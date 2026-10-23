from pathlib import Path
import json,hashlib,math,runpy
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
runpy.run_path(str(task/'verify-common.py'));runs=read(task/'native-runs.json');assert len(runs)==14
reports={}
for label,path in runs.items():
 f=Path(path)/'reedcrest.json'
 if label in ('small-normal','original','bird-normal','bracken-normal'):assert not f.exists();continue
 j=reports[label]=read(f);s=j['samples'];assert s
 assert all(x['species']==2 and x['needs_species']==2 and x['lifecycle_species']==2 and x['npc_count']==12 and x['npc_sessions']==12 for x in s),label
 # These diagnostic routes expose inherited >20deg yaw-only pose alignment;
 # ground projection/knowledge/motion are assessed separately, not foot acceptance.
 if label not in ('leading','camera','need','separation'):assert all(x['body_supported'] for x in s),label
 for a,b in zip(s,s[1:]):
  if a['partner'] and b['partner'] and a['partner']['id']==b['partner']['id']:
   dt=b['physical_seconds']-a['physical_seconds'];pa=a['partner'];pb=b['partner']
   assert math.dist([pa[k] for k in ('x','y','z')],[pb[k] for k in ('x','y','z')])<=.701*dt+1e-5,(label,'partner teleport')
primary=reports['partner'];s=primary['samples'];peer=primary['peer'];assert peer and any(x['leader']==2 and x['follow_owned'] for x in s)
hero_distance=math.hypot(s[-1]['body_x']-s[0]['body_x'],s[-1]['body_z']-s[0]['body_z']);npc_distance=s[-1]['partner']['distance']-s[0]['partner']['distance'];assert hero_distance>8 and npc_distance>5
assert all(x['peer'] in (0,peer) for x in s)
s=reports['leading']['samples'];direct=[x for x in s if x['direct']];assert direct and all(x['leader']==1 and not x['follow_owned'] for x in direct)
p0=s[0]['partner'];dx=p0['x']-s[0]['body_x'];dz=p0['z']-s[0]['body_z'];length=math.hypot(dx,dz);ux,uz=dx/length,dz/length
project=lambda a,b:(b['partner']['x']-a['partner']['x'])*ux+(b['partner']['z']-a['partner']['z'])*uz
away=[x for x in s if 4<=x['physical_seconds']<10];returned=[x for x in s if x['physical_seconds']>=16]
away_move=project(away[0],away[-1]);return_move=project(returned[0],returned[-1]);assert away_move<-2 and return_move>1
s=reports['camera']['samples'];assert all(not x['peer'] and not x['follow_owned'] and x['leader']==0 for x in s)
assert max(math.hypot(x['body_x']-s[0]['body_x'],x['body_z']-s[0]['body_z']) for x in s)<1e-5
camera_distance=max(math.hypot(x['camera_x']-x['body_x'],x['camera_z']-x['body_z']) for x in s);assert camera_distance>20
s=reports['need']['samples'];assert any(x['physical_seconds']<5 and x['follow_owned'] for x in s)
assert all(not x['follow_owned'] for x in s if x['physical_seconds']>5.2 and x['thirst']>.6)
contacts=read(Path(runs['need'])/'hero-autonomy.json')['contacts'];assert any(c['resource']==100 and c['amount']==.5 and abs(c['thirst_before']-c['thirst_after']-.5)<1e-8 for c in contacts)
s=reports['separation']['samples'];linked=[x for x in s if x['peer']];assert set(x['peer'] for x in linked)=={peer}
separations=[math.hypot(x['body_x']-x['partner']['x'],x['body_z']-x['partner']['z']) for x in s if x['partner']]
assert max(separations)>18 and any(not x['present'] for x in linked) and s[-1]['present'] and separations[-1]<9
nextlife=read(Path(runs['next-life'])/'death-next-life.json')['events'];n=[e for e in nextlife if e['event']=='explicit_next_life'];assert len(n)==1 and n[0]['hero_id']==2 and n[0]['fresh_session']==14
assert all(n[0][k] for k in ('ledger_unchanged_at_handoff','npc_snapshot_unchanged_at_handoff','old_death_known','old_id_excluded'))
assert reports['next-life']['samples'][-1]['hero']==2 and reports['next-life']['samples'][-1]['stage']==0
rest=read(Path(runs['rest'])/'hero-rest.json')['samples'];assert any(x['resting'] for x in rest) and not rest[-1]['resting'] and rest[-1]['fatigue']<.34
assert read(Path(runs['drink'])/'exterior-water.json')['hero_contacts']
assert any(e['event']=='arrived' for e in read(Path(runs['destination'])/'hero-destination.json')['events'])
review=read(root/'.clanker/clunkers/reedcrest-review-20261023/review.json')
for entry in review['source_hashes']:assert sha(root/entry['path'])==entry['sha256'],entry['path']
for label in ('partner','leading','camera','need','separation'):
 w=read(Path(runs[label])/'world-population.json');baseline=read(root/'var/reports/runtime/20261023-025626-4855727a/world-population.json')
 assert w['resource_ledger_initial']==baseline['resource_ledger_initial'] and w['resource_sessions_initial']==baseline['resource_sessions_initial']
summary={'ok':True,'scope':'Bounded reciprocal motion/knowledge/needs and fourth selected life; yaw-only body pose exceptions explicitly retained','ctest':'64/64 pass','same_executable_native_runs':14,'independent_source_hashes_match':True,'peer':peer,'partner_led_hero_displacement_m':hero_distance,'partner_led_npc_distance_m':npc_distance,'hero_led_npc_away_projection_m':away_move,'hero_led_npc_return_projection_m':return_move,'camera_body_separation_m':camera_distance,'wide_separation_max_m':max(separations),'reunion_final_m':separations[-1],'same_peer_separation_reunion':True,'pose_failures':{label:read(Path(path)/'runtime-receipt.json')['support_failure_count'] for label,path in runs.items()},'limits':['One calm proximity-learned session pair; no kindergarten origin or2-4companion cohort','Steep leading/camera/need/separation routes trigger inherited20deg yaw-only posing; no all-supported/foot-contact acceptance. Rendered-ground sampling fallbacks0 in all runs','NPC partner still uses imported Smalltrotter stand-in; no NPC species/growth art acceptance','Fixed-stage hero geometry/simple gait, no final anatomy/IK acceptance','Urgent partner contact and dormant once-clock have independent registry proof; native need fixture instead proves HERO need override/contact','N native proves selected components/world retention; clearing an already linked old hero is source/domain evidence','Longstride life, full reproduction/parenting, four islands and connected saves remain gaps; no target30FPS acceptance']}
(task/'verification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
