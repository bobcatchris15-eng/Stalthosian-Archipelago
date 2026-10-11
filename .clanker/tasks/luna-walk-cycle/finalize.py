import hashlib, json
from datetime import datetime, timezone
from pathlib import Path
root = Path('.').resolve()
task = root/'.clanker/tasks/luna-walk-cycle'
var = root/'var/luna-walk-cycle'
build = json.loads((task/'build-creature.log').read_text(encoding='utf-8'))
rig = json.loads((task/'rig-creature.log').read_text(encoding='utf-8'))
meta_path = var/'runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.json'
meta = json.loads(meta_path.read_text(encoding='utf-8'))
artifacts = {}
for item in rig.get('artifacts', []):
    path = Path(item['path'])
    if path.is_file():
        artifacts[item['role']+':'+path.name] = {
            'path': str(path.relative_to(root)) if path.is_relative_to(root) else str(path),
            'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        }
evidence = {
    'task_id':'luna-walk-cycle', 'base_revision':'f72f3b5b20de8bb40a8d4b7310592c745dee0a3c',
    'commands': {
        'build_creature': {'ok':build.get('ok'), 'exit_code':build.get('exit_code'), 'duration_s':build.get('duration_s'), 'tool_versions':build.get('metrics',{}).get('tool_versions')},
        'rig_creature': {'ok':rig.get('ok'), 'exit_code':rig.get('exit_code'), 'duration_s':rig.get('duration_s'), 'walk_cycle':rig.get('metrics',{}).get('walk_cycle')},
        'offline_tests': {'command':'python -m pytest -q stalthos-toolkit/tests/test_blender.py stalthos-toolkit/tests/test_gait.py','passed':True,'summary':'69 passed in 132.29s','log':'.clanker/tasks/luna-walk-cycle/focused-tests.log'},
        'gait_helper_tests': {'command':'python -m pytest -q stalthos-toolkit/tests/test_gait.py','passed':True,'summary':(task/'gait-tests.log').read_text(encoding='utf-8').strip(),'log':'.clanker/tasks/luna-walk-cycle/gait-tests.log'},
        'blender_52_legacy_action_failure': '.clanker/tasks/luna-walk-cycle/initial-failure.txt',
        'geometry_inspection': '.clanker/tasks/luna-walk-cycle/geometry-inspection.log',
    },
    'motion_metadata': meta,
    'artifacts': artifacts,
    'limitations':['Geometric stance contact fails the authored 0.02 m threshold.','Clip has no actor stride translation; in-place slip is diagnostic, not planted-foot proof.','Proxy tail clearance and foot IK remain unresolved.','No visual, native playback, target-hardware FPS, or full draft acceptance is claimed.'],
}
(task/'evidence.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
acceptance = [
 {'condition':'Pure gait helper produces deterministic cyclic phase/contact plans and passes offline unit tests.','met':True,'how_checked':'python -m pytest -q stalthos-toolkit/tests/test_blender.py stalthos-toolkit/tests/test_gait.py; 69 passed in 132.29s; focused test log at .clanker/tasks/luna-walk-cycle/focused-tests.log'},
 {'condition':'rig_creature consumes animation_contacts.walk_cycle and emits neutral plus animated GLBs and machine-readable motion metadata.','met':True,'how_checked':'Final build_creature and rig_creature envelopes plus emitted files under var/luna-walk-cycle; neutral GLB has 0 animations; walk GLB has 1 animation and 222 channels.'},
 {'condition':'Actual Blender output has valid animation channels and loop/contact evidence is recorded.','met':True,'how_checked':'Blender 5.2.0 rig_creature run; stalthos.optimize.glb validation passed; motion JSON records exact loop boundary and foot-pad sample results. Geometric contact result is false.'},
 {'condition':'Both hind foot pads remain within authored 0.02 m ground tolerance for all sampled stance frames, with planted horizontal motion.','met':False,'how_checked':'Motion JSON: 9 stance samples each, 0/9 within tolerance, gaps 0.646-0.665 m; in-place stance slip up to 0.104 m/0.079 m and no actor stride translation.'},
 {'condition':'No visual quality, Wicked import, or target hardware FPS acceptance is claimed without evidence.','met':True,'how_checked':'Task report and motion metadata explicitly state limitations.'},
]
receipt = {
 'task_id':'luna-walk-cycle','outcome':'partial','acceptance':acceptance,
 'artifacts_changed':[
  'stalthos-toolkit/src/stalthos_toolkit/blender/scripts/rig.py',
  'stalthos-toolkit/src/stalthos_toolkit/blender/scripts/gait.py',
  'stalthos-toolkit/src/stalthos_toolkit/commands/rig_creature.py',
  'stalthos-toolkit/tests/test_blender.py','stalthos-toolkit/tests/test_gait.py',
  '.clanker/tasks/luna-walk-cycle/brief.md','.clanker/tasks/luna-walk-cycle/task-state.json',
  '.clanker/tasks/luna-walk-cycle/receipt.json','.clanker/tasks/luna-walk-cycle/report.md',
  '.clanker/tasks/luna-walk-cycle/evidence.json','.clanker/tasks/luna-walk-cycle/focused-tests.log',
  '.clanker/tasks/luna-walk-cycle/gait-tests.log','.clanker/tasks/luna-walk-cycle/build-creature.log',
  '.clanker/tasks/luna-walk-cycle/rig-creature.log','.clanker/tasks/luna-walk-cycle/initial-failure.txt',
  '.clanker/tasks/luna-walk-cycle/inspect_geometry.py','.clanker/tasks/luna-walk-cycle/geometry-inspection.log','.clanker/tasks/luna-walk-cycle/finalize.py',
  'var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.neutral.glb',
  'var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb',
  'var/luna-walk-cycle/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.json'],
 'evidence':['.clanker/tasks/luna-walk-cycle/evidence.json','.clanker/tasks/luna-walk-cycle/focused-tests.log','.clanker/tasks/luna-walk-cycle/build-creature.log','.clanker/tasks/luna-walk-cycle/rig-creature.log','.clanker/tasks/luna-walk-cycle/initial-failure.txt'],
 'findings':['The YAML selectors bone:pes_foot_l/r do not exist in this proxy skeleton; they are explicitly aliased to toe_rear_L/R.','The actual foot pads are 8-vertex boxes embedded in body_proxy; measuring toe bone tails under-reports pad-to-ground gap by about 0.035 m.','The remaining lowest body point is the rear tail at z=0.469616 m while rear pad bottoms are z=0.652438 m; translating the full asset down to ground the pads would put the tail below the plane.','An in-place cycle cannot establish planted world-space feet without actor stride translation and foot IK.'],
 'blockers':['Foot-pad stance contact remains 0.646-0.665 m above the declared z=0 plane and horizontal stance slip reaches 0.104 m. Follow-up requires foot IK/stride handling and tail clearance review.'],
 'assumptions_made':['Mapped the authored left/right pes selectors to hind toe bones because no matching pes_foot_l/r bones exist.','Used generated foot-pad bottom vertices, not toe-bone endpoints, as the contact surface.','Measured against the current z=0 scene plane; did not translate the model to conceal contact failure.','The 2.0 second cycle repeats at the runtime consumer; start/end keys are equal and the GLB animation contains one clip.'],

}
(task/'receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
state = {
 'schema_version':1,'task_id':'luna-walk-cycle','status':'blocked',
 'base_revision':'f72f3b5b20de8bb40a8d4b7310592c745dee0a3c',
 'current_step':'verify-and-report','completed_steps':['read instructions and prior readiness report','implemented gait helper and conditional animated GLB export','ran focused Blender and offline tests','measured neutral foot pads, toe endpoints, and stance motion','saved final artifact and evidence receipt'],
 'next_step':'Resolve foot-pad contact with an accepted foot IK/stride model and validate tail clearance before marking gait contact accepted.',
 'touched_artifacts':receipt['artifacts_changed'],
 'validation':{'passed':False,'command':'python -m pytest -q stalthos-toolkit/tests/test_blender.py stalthos-toolkit/tests/test_gait.py','evidence_path':'.clanker/tasks/luna-walk-cycle/evidence.json'},
 'blockers':receipt['blockers'],'assumptions':receipt['assumptions_made'],
 'receipt_path':'.clanker/tasks/luna-walk-cycle/receipt.json',
 'resume_instructions':'Start by inspecting this worktree at the walk-cycle handoff. The neutral and animated GLBs and their measured evidence are under var/luna-walk-cycle. The source implementation and focused tests are already complete and passed. Continue with the unresolved foot-pad ground contact and in-place slip; do not mark contact accepted until evaluated stance pad vertices remain within tolerance and horizontal motion is proven. Preserve the current failed evidence.',
 'updated':datetime.now(timezone.utc).isoformat()
}
(task/'task-state.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
