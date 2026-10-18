"""Bounded STL8 native-cap fixture and exact-history audit; source save stays immutable."""
import sys,struct,json,hashlib
from pathlib import Path

def decode(path):
 data=path.read_bytes(); o=4
 def read(fmt):
  nonlocal o
  v=struct.unpack_from('<'+fmt,data,o);o+=struct.calcsize('<'+fmt)
  return v[0] if len(v)==1 else v
 assert data[:4]==b'STL8' and read('I')==8
 times=read('dd');read('QQ')
 if read('B'):read('QQ')
 pose=read('ddddB') if read('B') else None
 if read('B'):read('QBdB')
 if read('B'):read('QBddddB')
 assert read('B')==1
 start=o
 resources=[read('QBdd') for _ in range(read('I'))]
 sessions=[read('QQd') for _ in range(read('I'))]
 end=o
 if read('B'):read('BBdddddd')
 assert read('B')==1
 read('QQQ');actors=[read('QBdddddQQdBddddBB') for _ in range(read('I'))]
 mappings=[read('QQ') for _ in range(read('I'))]
 return dict(data=data,start=start,end=end,resources=resources,sessions=sessions,actors=actors,mappings=mappings,times=times,pose=pose)

def fixture(source,target):
 d=decode(source);sessions=d['sessions'];actors=d['actors']
 # Remove one actively browsing actor's prior contacts; retained depletion remains.
 rejected=next(a[0] for a in actors if a[-1]==2)
 session=dict(d['mappings'])[rejected]
 sessions=[s for s in sessions if s[0]!=session]
 count=256-len(sessions); assert 0<count<=256
 resources=d['resources']+[(4095,0,1000.,float(count))]
 sessions=sessions+[(10000+i,4095,1.) for i in range(count)]
 block=struct.pack('<I',len(resources))+b''.join(struct.pack('<QBdd',*r) for r in resources)
 block+=struct.pack('<I',len(sessions))+b''.join(struct.pack('<QQd',*s) for s in sessions)
 target.write_bytes(d['data'][:d['start']]+block+d['data'][d['end']:])
 result={'source_sha256':hashlib.sha256(d['data']).hexdigest(),'fixture_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'blocked_actor':rejected,'blocked_session':session,'history_keys':256,'fixture_scope':'copy-only extra inert shrub4095 and synthetic historical keys; one browser key absent'}
 target.with_suffix('.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))

def audit(before,after,evidence):
 a,b=decode(before),decode(after);p=json.loads((evidence/'world-population.json').read_text())
 old={(s,r):v for s,r,v in a['sessions']};new={(s,r):v for s,r,v in b['sessions']}
 assert len(old)==len(new)==256 and old.keys()==new.keys()
 assert all(new[k]>=v for k,v in old.items())
 assert a['mappings']==b['mappings'] and len(a['mappings'])==8
 assert p['resource_history_rejections']>0 and p['resource_history_available']==0
 assert p['food_consumed']>0 or p['water_consumed']>0
 assert p['needs_relief_requires_accepted_contact'] is True
 fixtureInfo=json.loads(before.with_suffix('.json').read_text());blocked=fixtureInfo['blocked_actor']
 ai=next(x for x in p['actors_initial'] if x['id']==blocked);bi=next(x for x in p['actors_final'] if x['id']==blocked)
 assert bi['hunger']>ai['hunger']
 result={'passed':True,'keys_preserved':256,'mappings_preserved':8,'rejected_contacts':p['resource_history_rejections'],'accepted_food':p['food_consumed'],'accepted_water':p['water_consumed'],'blocked_actor_hunger_initial':ai['hunger'],'blocked_actor_hunger_final':bi['hunger'],'evidence':str(evidence)}
 print(json.dumps(result));(evidence/'history-cap-audit.json').write_text(json.dumps(result,indent=2))

if __name__=='__main__':
 if sys.argv[1]=='fixture':fixture(Path(sys.argv[2]),Path(sys.argv[3]))
 elif sys.argv[1]=='audit':audit(Path(sys.argv[2]),Path(sys.argv[3]),Path(sys.argv[4]))
