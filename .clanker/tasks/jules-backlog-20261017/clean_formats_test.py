from pathlib import Path
p=Path('var/worktrees/jules-backlog-20261017/stalthos-toolkit/tests/test_formats.py'); t=p.read_text(); t='\n'.join(x.rstrip() for x in t.splitlines())+'\n'; t=t.replace('from pathlib import Path\n',''); p.write_text(t,encoding='utf-8')
