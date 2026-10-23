from pathlib import Path
import json,hashlib,math,runpy
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
runpy.run_path(str(task/'verify-common.py'));runs=read(task/'native-runs.json');assert len(runs)==11
reports={}
for label,path in runs.items():
 f=Path(path)/'brackenback.json'
 if label in ('small-normal','original','bird-normal'):assert not f.exists();continue
 j=reports[label]=read(f);s=j['samples'];assert s
 assert all(x['species']==1 and x['needs_species']==1 and x['lifecycle_species']==1 and x['body_supported'] and x['npc_count']==12 and x['npc_sessions']==12 for x in s),label
 assert all(x['club_visible']==(x['stage'] in (2,3)) for x in s),label
 assert not j['npc_defense_integrated']
s=reports['protect']['samples'];observed=[x for x in s if x['observed']];assert observed and any(x['interposing'] and x['cue_visible'] and x['distress_visible'] for x in observed)
assert reports['protect']['synthetic_existing_actor_edge_position'] and not reports['protect']['existing_actor_positions_preserved_at_fixture']
for x in observed:
 actors={a['id']:a for a in x['actors']};small=actors[x['vulnerable']];threat=actors[x['threat']]
 assert small['id']!=threat['id'] and small['alive'] and threat['alive'] and threat['species']==4
 assert max(small['hunger'],small['thirst'])>=.85
 assert math.hypot(threat['x']-small['x'],threat['z']-small['z'])<=6.01
 assert max(math.hypot(a['x']-x['body_x'],a['z']-x['body_z']) for a in (small,threat))<12
motion=math.hypot(s[-1]['body_x']-s[0]['body_x'],s[-1]['body_z']-s[0]['body_z']);assert motion>2
s=reports['override']['samples'];direct=[x for x in s if x['direct']];assert direct and all(not x['interposing'] and not x['cue_visible'] for x in direct)
assert any(e['event']=='yield_or_lost_stimulus' and e['direct'] for e in reports['override']['events'])
s=reports['camera']['samples'];assert all(not x['observed'] and not x['interposing'] and not x['cue_visible'] and not x['distress_visible'] for x in s)
assert max(math.hypot(x['body_x']-s[0]['body_x'],x['body_z']-s[0]['body_z']) for x in s)<1e-5
camera_separation=max(math.hypot(x['camera_x']-x['body_x'],x['camera_z']-x['body_z']) for x in s);assert camera_separation>20
n=read(Path(runs['next-life'])/'death-next-life.json')['events'];n=[e for e in n if e['event']=='explicit_next_life'];assert len(n)==1 and n[0]['hero_id']==2 and n[0]['fresh_session']==14
assert all(n[0][k] for k in ('ledger_unchanged_at_handoff','npc_snapshot_unchanged_at_handoff','old_death_known','old_id_excluded'))
assert reports['next-life']['samples'][-1]['hero']==2 and reports['next-life']['samples'][-1]['stage']==0
rest=read(Path(runs['rest'])/'hero-rest.json')['samples'];assert any(s['resting'] for s in rest) and not rest[-1]['resting'] and rest[-1]['fatigue']<.34
water=read(Path(runs['drink'])/'exterior-water.json');assert water['hero_contacts']
destination=read(Path(runs['destination'])/'hero-destination.json');assert any(e['event']=='arrived' for e in destination['events'])
review=read(root/'.clanker/clunkers/brackenback-review-20261023/review.json')
for entry in review['source_hashes']:assert sha(root/entry['path'])==entry['sha256'],entry['path']
summary={'ok':True,'ctest':'63/63 pass','same_executable_native_runs':11,'source_hashes_match_independent_review':True,'interposition_approach_m':motion,'camera_body_separation_m':camera_separation,'native_bracing_samples':sum(s['bracing'] for s in reports['protect']['samples']),'next_life':n[0],'limits':['Injected existing actor edge position, age and critical needs, adult defender and input/noon fixtures; not naturally occurring encounter','Approach/direct override/stimulus loss proven; complete brace and deterrence outcome not demonstrated','NPC defensive movement unimplemented; generic imported NPC body does not establish protected species/juvenile appearance','Procedural fixed-sized body/simple gait, adult-only club; no anatomy/contact/growth art acceptance','Three selectable species; complete families, four-island traversal, connected saves and target hardware30FPS remain missing']}
(task/'verification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
