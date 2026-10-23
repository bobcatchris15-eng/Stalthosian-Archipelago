from pathlib import Path
import json,hashlib
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-20261023'
b=json.loads((t/'preservation-before.json').read_text(encoding='utf-8-sig'))
changed=[p for p,h in b.items() if not (r/p).is_file() or hashlib.sha256((r/p).read_bytes()).hexdigest()!=h]
(t/'preservation-after.json').write_text(json.dumps({'paths_checked':len(b),'changed':changed,'passed':not changed},indent=2)+'\n')
assert not changed,changed
print('PRESERVATION_PASS',len(b))
