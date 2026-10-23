from pathlib import Path
import json,hashlib,subprocess
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-repair-20261023';entries=json.loads((r/'.clanker/clunkers/native-island-crossing-review-20261023/repair-source-hashes.json').read_text(encoding='utf-8'))['sources'];results=[]
for e in entries:
 raw=(r/e['path']).read_bytes();canonical=hashlib.sha256(raw.replace(bytes([13,10]),bytes([10]))).hexdigest();blob=subprocess.check_output(['git','hash-object','--path='+e['path'],e['path']],cwd=r).decode().strip();results.append({'path':e['path'],'canonical_match':canonical==e['canonical_lf_sha256'],'blob_match':blob==e['git_blob']})
(t/'source-verification.json').write_text(json.dumps(results,indent=2)+'\n');assert len(results)==14 and all(x['canonical_match'] and x['blob_match'] for x in results),results
print('14_SOURCE_CANONICAL_AND_BLOBS_MATCH')
