import json,pathlib,hashlib
root=pathlib.Path('.');task=root/'.clanker/tasks/exterior-woodland-20261020';reports=root/'var/reports/runtime'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def report(name,file):return read(reports/name/file)
proxy='20261020-093656-cc82e1f1';animal='20261020-093751-88e2ca44';normal='20261020-093841-18911a09';original='20261020-093939-23a2bee1'
metrics={}
for name in [proxy,animal,normal]:
 r=report(name,'runtime-receipt.json');j=report(name,'day-night.json');s=j['samples']
 assert r['ok'] and r['support_failure_count']==0 and r['rendered_ground_support_fallbacks']==0
 assert r['woodland_proposals_placed']==10 and r['woodland_proposals_skipped']==0 and r['woodland_ground_fallbacks']==0
 assert r['woodland_adult_placed']==6 and r['woodland_sapling_placed']==4 and r['woodland_unique_objects']==4
 assert r['woodland_ground_sample_source']=='resident_streamed_rendered_triangles'
 assert r['woodland_drawn_triangles']==36840 and r['woodland_unique_triangles']==7616
 trees=r['woodland_placed_instances'];assert {v['id'] for v in trees}==set(range(9000,9010))
 assert all(v['rendered_ground_sampled'] and v['ground_delta_m']==0 and abs(v['z']-47)>=7 for v in trees)
 assert all(abs(v['physical_seconds']-v['calendar_seconds'])<1e-9 for v in s)
 if name!=normal:
  assert j['woodland_entry_probe'] and j['synthetic_renderer_phase']
  assert all(v['rendered_fraction']==.5 and v['body_z']==47 for v in s)
  assert all(v['saved'] for v in j['captures']) and len(j['captures'])==4
 else:
  assert not j['woodland_entry_probe'] and not j['synthetic_renderer_phase']
  assert all(abs(v['rendered_fraction']-v['calendar_fraction'])<1e-12 for v in s)
 metrics[name]={'body_start_x':s[0]['body_x'],'body_end_x':s[-1]['body_x'],'displacement_m':s[-1]['body_x']-s[0]['body_x'],'support_failures':0,'support_fallbacks':0,'placed':10,'shared_unique_objects':4,'unique_triangles':7616,'drawn_triangles':36840}
assert metrics[proxy]['body_end_x']>=184 and metrics[proxy]['displacement_m']>37.9
assert metrics[animal]['body_end_x']>150 and metrics[animal]['displacement_m']>4.5
w=report(normal,'world-population.json');b=report('20261020-083805-d367069b','world-population.json')
assert w['actors_initial']==b['actors_initial'] and len(w['actors_initial'])==12
assert w['resource_ledger_initial']==b['resource_ledger_initial'] and w['resource_sessions_initial']==b['resource_sessions_initial']
assert report(original,'runtime-receipt.json')['ok']
pin=read(root/'stalthos-runtime/config/connected-woodland.json');old=read(task/'initial-needle-assets.json')
for group in [pin,old]:
 for stage in ['adult','sapling']:
  f=root/group[stage]['path'];assert hashlib.sha256(f.read_bytes()).hexdigest()==group[stage]['sha256']
prior=read(task/'preservation-before.json');changed=[]
for path,digest in prior.items():
 if path=='.clanker/graph/built-at-rev':continue
 f=root/path
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=digest:changed.append(path)
assert not changed,changed
metrics['preserved_original_untracked_files']=len(prior)-1
metrics['baseline']='original8/all12 actors and original11 resource/session initial states exactly preserved'
metrics['visual_review']='root inspected proxy and actual animal native images; broad-foliage stand-in recognizable; sparse crowns and coarse ground remain; needle-spray assets retained untouched'
metrics['scope']='labelled diagnostic start/input/noon; existing ordinary supported motion, not human physical input, full animal crossing, forest ecology, physical canopy shelter or FPS'
(task/'verification.json').write_text(json.dumps(metrics,indent=2))
print('EXTERIOR_WOODLAND_NATIVE_VERIFIED proxy',metrics[proxy]['displacement_m'],'m; animal',metrics[animal]['displacement_m'],'m;10trees and preservation checked')
