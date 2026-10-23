from pathlib import Path
import json,re
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-repair-20261023';matches={}
for label in ('old-save','destination'):
 a=(t/(label+'-before.sha256')).read_text().strip();b=(t/(label+'-after.sha256')).read_text().strip();assert a==b;matches[label]=a
p=Path(re.search(r'evidence (.+)',(t/'old-config-refusal.log').read_text()).group(1).strip());failure=json.loads((p/'failure.json').read_text());assert failure['ok'] is False and 'configuration' in failure['error'];(t/'refusal-verification.json').write_text(json.dumps({'ok':True,'failure':failure,'path':str(p),'unchanged_hashes':matches,'older_partial_checkpoint_retained':True,'existing_destination_retained':True},indent=2)+'\n');print('OLD_CONFIG_REFUSED_BYTES_RETAINED')
