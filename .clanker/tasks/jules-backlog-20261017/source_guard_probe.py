import importlib.util,pathlib,types,contextlib,io,json,tempfile
root=pathlib.Path.cwd(); spec=importlib.util.spec_from_file_location('seamreview',root/'stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py'); m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='safe-seam-review-',dir=root/'var/reports/jules-backlog-20261017') as td:
 source=pathlib.Path(td)/'fake-master.blend';source.write_bytes(b'FAKE MASTER RECEIPT GUARD PROBE')
 body=types.SimpleNamespace(name='Smalltrotter_Body',type='MESH',library=None,data=types.SimpleNamespace(library=None),modifiers=[])
 m.bpy=types.SimpleNamespace(data=types.SimpleNamespace(objects={'Smalltrotter_Body':body}),ops=types.SimpleNamespace(wm=types.SimpleNamespace(open_mainfile=lambda **kw:None)),context=types.SimpleNamespace(scene=types.SimpleNamespace(objects=[])))
 m.collect_body=lambda b:{'topology':{},'component_ids':{}}
 m.parse_args=lambda:types.SimpleNamespace(selftest=False,input=str(source),output=None,receipt=str(source),mode='audit')
 before=source.read_bytes(); stdout=io.StringIO()
 with contextlib.redirect_stdout(stdout):code=m.main()
 print(json.dumps({'exit_code':code,'source_changed':source.read_bytes()!=before,'source_now_json':source.read_text().startswith('{'),'reported_success':json.loads(stdout.getvalue())['success']}))
