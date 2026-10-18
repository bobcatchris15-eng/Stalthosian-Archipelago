import subprocess,json,pathlib,re,concurrent.futures,hashlib,os
root=pathlib.Path.cwd(); out=root/'var/reports/jules-backlog-20261017'; cli=root/'var/tools/jules/jules.exe'
r=subprocess.run([str(cli),'remote','list','--session'],capture_output=True)
(out/'sessions.txt').write_bytes(r.stdout)
rows=[]
for line in r.stdout.decode('utf-8','replace').splitlines():
 m=re.match(r'\s*(\d+)\s+(.*)',line)
 if not m: continue
 sid,desc=m.groups()
 if sid=='1630345306947286607': break
 rows.append({'id':sid,'listing':desc})
def pull(row):
 p=subprocess.run([str(cli),'remote','pull','--session',row['id']],capture_output=True)
 target=out/(row['id']+'.patch'); target.write_bytes(p.stdout)
 (out/(row['id']+'.stderr.txt')).write_bytes(p.stderr)
 row.update(exit_code=p.returncode,bytes=len(p.stdout),sha256=hashlib.sha256(p.stdout).hexdigest(),files=re.findall(r'^diff --git a/(.+?) b/',p.stdout.decode('utf-8','replace'),re.M))
 return row
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool: rows=list(pool.map(pull,rows))
inv={'base':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'api_key_present':bool(os.environ.get('JULES_API_KEY')),'sessions':rows}
(out/'inventory.json').write_text(json.dumps(inv,indent=2),encoding='utf-8')
print(json.dumps(inv,indent=2))
