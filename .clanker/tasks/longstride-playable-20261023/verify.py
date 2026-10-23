from pathlib import Path
import json,hashlib,math,runpy,subprocess
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'));sha=lambda b:hashlib.sha256(b).hexdigest()
runpy.run_path(str(task/'verify-common.py'));runs=read(task/'native-runs.json');assert len(runs)==13
reports={}
for label,path in runs.items():
 f=Path(path)/'longstride.json'
 if label in ('small-normal','original','bird-normal','bracken-normal','reed-normal'):assert not f.exists();continue
 j=reports[label]=read(f);s=j['samples'];assert s
 assert j['authored_carrion_not_dead_npc'] and not j['npc_carrion_feeding_integrated']
 assert all(x['species']==4 and x['needs_species']==4 and x['lifecycle_species']==4 and x['body_supported'] and x['npc_count']==12 and x['npc_sessions']==12 for x in s),label
 for c in j['contacts']:
  if 'hunger_before' in c:assert c['resource']==211,label
for label in ('feed-life','inspection'):
 j=reports[label];c=[x for x in j['contacts'] if x['resource']==211];assert len(c)==2 and [x['session'] for x in c]==[13,14]
 assert all(x['eligible'] and x['amount']==.5 and abs(x['hunger_before']-x['hunger_after']-.5)<1e-8 for x in c)
 s=j['samples'];assert j['remaining']==0 and s[-1]['remaining']==0 and not s[-1]['visible_flesh'] and s[-1]['fraction']==0
 n=read(Path(runs[label])/'death-next-life.json')['events'];n=[x for x in n if x['event']=='explicit_next_life'];assert len(n)==1 and n[0]['hero_id']==2 and n[0]['fresh_session']==15
 assert all(n[0][k] for k in ('ledger_unchanged_at_handoff','npc_snapshot_unchanged_at_handoff','old_death_known','old_id_excluded'))
 assert s[-1]['hero']==2 and s[-1]['session']==15 and s[-1]['stage']==0
 ledger=read(Path(runs[label])/'world-population.json')['resource_ledger_final'];food=[r for r in ledger['resources'] if r['id']==211];assert len(food)==1 and food[0]['consumed_quantity']==1
 assert all(k['session_id'] in (13,14) for k in ledger['sessions'] if k['resource_id']==211)
 assert all(c['saved'] for c in j['captures']) and len(j['captures'])>=4
s=reports['override']['samples'];direct=[x for x in s if x['direct']];assert direct and all(not x['auto_forward'] and not x['auto_contact'] for x in direct)
c=[x for x in reports['override']['contacts'] if x['resource']==211];assert len(c)==1 and reports['override']['remaining']==.5
s=reports['camera']['samples'];assert all(not x['attention'] and not x['auto_contact'] and not x['auto_forward'] for x in s)
assert not reports['camera']['contacts'] and reports['camera']['remaining']==1
assert max(math.hypot(x['body_x']-s[0]['body_x'],x['body_z']-s[0]['body_z']) for x in s)<1e-6
camera=max(math.hypot(x['camera_x']-x['body_x'],x['camera_z']-x['body_z']) for x in s);assert camera>20
rest=read(Path(runs['rest'])/'hero-rest.json')['samples'];assert any(x['resting'] for x in rest) and not rest[-1]['resting'] and rest[-1]['fatigue']<.34
assert read(Path(runs['drink'])/'exterior-water.json')['hero_contacts']
assert any(x['event']=='arrived' for x in read(Path(runs['destination'])/'hero-destination.json')['events'])
review=read(root/'.clanker/clunkers/longstride-review-20261023/review.json');raw_differences=[]
for entry in review['source_hashes']:
 data=(root/entry['path']).read_bytes();assert sha(data.replace(b'\r\n',b'\n'))==entry['canonical_lf_sha256']
 blob=subprocess.check_output(['git','hash-object','--path='+entry['path'],entry['path']],cwd=root).decode().strip();assert blob==entry['git_blob']
 if sha(data)!=entry['sha256']:raw_differences.append({'path':entry['path'],'integrated_raw_sha256':sha(data),'reviewed_raw_sha256':entry['sha256'],'canonical_lf_and_git_blob_equal':True})
summary={'ok':True,'ctest':'65/65 pass','same_executable_native_runs':13,'canonical_lf_and_git_blob_match_independent_review':True,'line_ending_only_raw_differences':raw_differences,'actual_feed_contacts':reports['feed-life']['contacts'],'camera_body_separation_m':camera,'depleted_food_retained_across_native_N':True,'successor':2,'fresh_session':15,'limits':['Authored finite food cohort using historical Shrub/genericfood ledger enum; not a killed NPC','No NPC carrionfeeding/speciesdiet, hunting, attacks or dependentyoung care','Procedural fixed-stage theropod/simple gait; no anatomical bite/foot/growth/finalart acceptance','Nativefixtures inject hunger/start/terminalstate/N/noon; no natural hunger/death pacing claim','Allfive startup lives have bounded roles, fullspecieslifecycles/reproduction/parenting remain gaps','Four islands/crossings and connected persistence still missing; no target30FPS acceptance']}
(task/'verification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
