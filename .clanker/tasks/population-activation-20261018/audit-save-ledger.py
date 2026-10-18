"""Read only the documented STL8 prefix for this native save continuity audit."""
import json,struct,pathlib,hashlib
root=pathlib.Path(__file__).resolve().parents[3]
report=root/'var/reports/population-activation-20261018'
def prefix(path):
 data=path.read_bytes(); offset=4
 def read(fmt):
  nonlocal offset
  value=struct.unpack_from('<'+fmt,data,offset);offset+=struct.calcsize('<'+fmt)
  return value[0] if len(value)==1 else value
 assert data[:4]==b'STL8' and read('I')==8
 times=read('dd');read('QQ')
 if read('B'):read('QQ')
 pose=read('ddddB') if read('B') else None
 if read('B'):read('QBdB')
 if read('B'):read('QBddddB')
 assert read('B')==1
 resources=[read('QBdd') for _ in range(read('I'))]
 sessions=[read('QQd') for _ in range(read('I'))]
 if read('B'):read('BBdddddd')
 assert read('B')==1
 read('QQQ');actors=[read('QBdddddQQdBddddBB') for _ in range(read('I'))]
 mappings=[read('QQ') for _ in range(read('I'))]
 return dict(times=times,pose=pose,resources=resources,sessions=sessions,mappings=mappings,actors=actors,sha256=hashlib.sha256(data).hexdigest())
runs={n:prefix(report/(n+'.stl')) for n in ('departure','dormant','return')}
a,b,c=[runs[n] for n in ('departure','dormant','return')]
assert a['resources']==b['resources'] and a['sessions']==b['sessions']
assert a['mappings']==b['mappings']==c['mappings'] and len(a['mappings'])==8
prior={(s,r):v for s,r,v in b['sessions']};after={(s,r):v for s,r,v in c['sessions']}
assert all(k in after and after[k]>=v for k,v in prior.items())
assert a['pose'][0]==b['pose'][0]==544 and c['pose'][0]==24
result=dict(passed=True,dormant_ledger_decoded_values_exact=True,actor_resource_mappings_exact=True,return_history_keys_preserved=True,history_keys_before=len(prior),history_keys_after=len(after),saves=runs)
(root/'.clanker/tasks/population-activation-20261018/save-ledger-audit.json').write_text(json.dumps(result,indent=2)+'\n')
print({k:v for k,v in result.items() if k!='saves'})
