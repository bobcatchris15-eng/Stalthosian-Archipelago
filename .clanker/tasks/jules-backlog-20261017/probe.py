import pathlib,subprocess,json
root=pathlib.Path.cwd(); out=root/'var/reports/jules-backlog-20261017'
probe='''import json\nfrom stalthos_toolkit.water import optics\nfrom stalthos_toolkit.schemas.formats import classify_locator\ntry:\n print("scattering="+str(optics.scattering_coefficient_m(535.0)))\nexcept Exception as e:\n print(type(e).__name__+": "+str(e))\nprint("doi="+str(classify_locator("https://doi.org/10.1234/example").normalized))'''
for label,base in [('baseline',root),('candidate',root/'var/worktrees/jules-backlog-20261017')]:
 import os
 env=dict(os.environ,PYTHONPATH=str(base/'stalthos-toolkit/src'))
 p=subprocess.run(['python','-c',probe],cwd=base,env=env,capture_output=True)
 (out/(label+'-probe.txt')).write_bytes(p.stdout+p.stderr)
 print(label,p.stdout.decode())
