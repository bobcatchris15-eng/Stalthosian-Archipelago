import json,pathlib,subprocess,collections
root=pathlib.Path.cwd(); out=root/'var/reports/jules-backlog-20261017'; inv=json.loads((out/'inventory.json').read_text()); wt=root/'var/worktrees/jules-backlog-20261017'
for r in inv['sessions']:
 if not r['files']: r['apply_status']='no_diff'; continue
 args=['git','-C',str(wt),'apply','--check',str(out/(r['id']+'.patch'))]
 p=subprocess.run(args,capture_output=True); q=subprocess.run(args[:-2]+['--reverse','--check',args[-1]],capture_output=True)
 r['apply_status']='applies' if p.returncode==0 else ('already_applied' if q.returncode==0 else 'conflicts')
 r['apply_check']=p.stderr.decode('utf-8','replace')
(out/'inventory.json').write_text(json.dumps(inv,indent=2),encoding='utf-8')
for r in inv['sessions']: print(r['id'],r['apply_status'],', '.join(r['files']))
print(collections.Counter(r['apply_status'] for r in inv['sessions']))
