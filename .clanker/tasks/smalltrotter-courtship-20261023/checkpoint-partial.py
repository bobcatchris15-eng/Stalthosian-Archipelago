from pathlib import Path
import json,hashlib,re,subprocess
r=Path.cwd();t=r/'.clanker/tasks/smalltrotter-courtship-20261023';read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
proof={}
for label in ['courtship','camera','hatch']:
 log=(t/f'native-{label}-final.log').read_text();p=Path(re.search(r'evidence (.+)',log).group(1).strip());j=read(p/'nesting.json');receipt=read(p/'runtime-receipt.json');assert receipt['ok'];proof[label]={'path':str(p),'clutch_mate':j['clutch_mate'],'offspring_admitted':j['offspring_admitted'],'events':j['events'],'courtship_events':j['courtship_events'],'hatch_events':j['hatch_events']}
assert proof['courtship']['clutch_mate']!=0
assert proof['camera']['clutch_mate']==0 and not any(e.get('invited') for e in proof['camera']['courtship_events'])
for path,h in read(t/'preservation-before.json').items():assert hashlib.sha256((r/path).read_bytes()).hexdigest()==h,path
review=read(r/'.clanker/clunkers/smalltrotter-courtship-review-20261023/review.json')
for e in review['source_hashes']:
 data=(r/e['path']).read_bytes();assert hashlib.sha256(data.replace(b'\r\n',b'\n')).hexdigest()==e['canonical_lf_sha256']
s=read(t/'task-state.json');s.update(current_step='Final executable passed 66 tests and three native probes; remaining matrix required before publication',resume_instructions='No source edits needed before remaining final-executable override/refusal/all-five/original regression packet. Read partial-native-verification.json; do not count older executable evidence. Then complete verify.py, source review, scan, checkpoint and ordinary publication. Connected persistence is next missing gameplay task after acceptance.');(t/'task-state.json').write_text(json.dumps(s,indent=2)+'\n')
(t/'partial-native-verification.json').write_text(json.dumps({'status':'partial_not_accepted_for_publication','tests':'66/66','native':proof,'preserved_paths':230,'reviewed_source_canonical_hashes_match':True,'composed_visual':'courtship nest-2.png inspected; actual mate and clutch with readable acceptance cue','remaining':'Final executable override/refusal and five normal/original regression packet, full verify.py, final receipt/publication'},indent=2)+'\n')
p=r/'.clanker/current-work.md';p.write_text('# Courtship source reviewed; final native matrix incomplete - 2026-10-23\n\nRead .clanker/tasks/smalltrotter-courtship-20261023/partial-native-verification.json and task-state.json first. Longstride fifth species published at40eddd9. Courtship source local HEADfa941f2, 66tests and three final native probes only; NOT publication accepted. Finish remaining final-executable packet before source changes or dispatch. Preserve all state. Next missing gameplay after acceptance: connected persistence per task next-task.md.\n\n'+p.read_text())
print(json.dumps({'partial_checkpoint':True,'births':proof['hatch']['offspring_admitted']}))
