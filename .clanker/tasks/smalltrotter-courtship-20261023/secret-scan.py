import subprocess,re,json
from pathlib import Path
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
parts=[subprocess.check_output(['git','diff','--no-ext-diff','--unified=0','origin/main','HEAD'],cwd=root,text=True,encoding='latin-1'),subprocess.check_output(['git','diff','--cached','--no-ext-diff','--unified=0'],cwd=root,text=True,encoding='latin-1')]
added='\n'.join(line[1:] for text in parts for line in text.splitlines() if line.startswith('+') and not line.startswith('+++'))
pattern=r"BEGIN [A-Z ]*PRIVATE KEY|AKIA[0-9A-Z]{16}|xox[baprs]-[0-9A-Za-z-]{10,}|gh[pousr]_[0-9A-Za-z]{36}|(?:api[_-]?key|secret|passwd|password|token)[\"' ]*[:=][\"' ]*[0-9A-Za-z/+_-]{16,}"
count=len(list(re.finditer(pattern,added,re.I)))
(task/'secret-scan.json').write_text(json.dumps({'scope':'ordinary publication origin/main..HEAD plus staged checkpoint','added_lines':len(added.splitlines()),'potential_secrets':count,'passed':count==0},indent=2)+'\n')
print('PUBLICATION_SECRET_SCAN_PASS' if not count else 'PUBLICATION_SECRET_SCAN_FAILED')
raise SystemExit(bool(count))
