from pathlib import Path
import json,hashlib,datetime,subprocess
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-20261023';read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
a=(t/'old-save-before.sha256').read_text().strip();b=(t/'old-save-after.sha256').read_text().strip();assert a==b
failure=read(r/'var/reports/runtime/20261023-132322-fa52c7ee/failure.json')
v=read(t/'native-verification.json');assert len(v['runs'])==6
receipt={'schema_version':1,'task':'native-island-crossing-20261023','disposition':'partial local construction checkpoint; NOT native accepted; NOT published','source_revision':'e55ffe4','review_revision':'c9711b3','build_receipt':'var/build/stalthos-runtime/build-receipt.json','executable_sha256':v['executable_sha256'],'ctest':'70/70 passed','native_verification':'native-verification.json','preservation':'253 unrelated untracked paths hash-identical','old_config_refusal':{'failure':failure,'old_file_sha256_unchanged':a},'verified':['Shared four-landform source/domain and actual native shore/wade/swim body movement','453 native swim samples: root-.55, no resting/no island arrival','Camera fixture body position fixed; no remote-island tour claim','Original STL9 save/restart and partial STLC save/restart pass; exact partial reconstructed owner before first tick','Independent exact source blobs,8645 protected-origin height comparisons and432 streaming boundary probes'],'native_failures':['Pinned Smalltrotter160s requested/158.18 physical ended at432.948,-313.503; Stone not reached; fatigue1 reduces swim speed to.88','11476 terrain-foot support failures crossing; camera stationary shore3567 too. Diagnose actual shore slope/stance and water modes, do not dismiss as only water diagnostics','Domain122.46s calculation omitted actual pinned land stride and environmental exposure; not native proof'],'visual_inspection':['Viewed actual composed island-crossing-1/2.png: visible body, shore, water and island silhouette plus location/water UI','Very dark bare coast, stiff airborne-looking walking body over sea; final scenery/swim animation not accepted'],'limits':['No physical all-four travel acceptance','No new-island populations or foundation region migration','No unrestricted connected persistence','No target hardware30FPS evidence','Longstride-selected island diagnostic startup ordering limitation'],'next_task':'next-task.md','publication':'held locally until native failures resolved'}
(t/'integration-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
s=read(t/'task-state.json');s.update(status='running',current_step='Partial local checkpoint: native crossing failed landing/support gates; repair before publication',completed_steps=['Source/domain implementation and independent exact-blob review integrated','Build and70CTest pass','Six native runs, partial exact reconstruction, original saved mode and old-file refusal checked','253 preserved paths verified; graph refreshed'],next_step='Resolve actual pinned crossing duration/fatigue and shore/water support; then rerun physical landing and regressions',validation={'passed':False,'command':'Build.ps1;70CTest;six native runs','evidence_path':str(t/'native-verification.json')},receipt_path=str(t/'integration-receipt.json'),resume_instructions='Read integration-receipt.json/native-verification.json/next-task.md first. Source e55ffe4+reviewc9711b3 local; do not publish until native landing/support failures resolved. Both workers collected, slots released. Preserve all saves/unrelated paths; no native process remains. Full four-island/ecology/region saves remain gaps.',updated=datetime.datetime.now(datetime.timezone.utc).isoformat());(t/'task-state.json').write_text(json.dumps(s,indent=2)+'\n')
notice='''# Partial native island construction checkpoint - 2026-10-23

Task .clanker/tasks/native-island-crossing-20261023/integration-receipt.json.
Shared four-landform geometry, physical water datum/fatigue and body island UI are
implemented and independently reviewed;70CTest pass. Native pinned160s run swims
but does NOT land on Stone, fatigue reaches1, and terrain-foot support failures
occur at shore and sea. Domain timing omitted actual pinned stride/exposure.
Source remains LOCAL, unaccepted/unpublished. Six native runs include original
STL9 and partial STLC restart; exact partial reconstructed owner matches. Older
partial config refused with exact bytes retained;253 unrelated paths unchanged.
NEXT repair actual pinned crossing balance/shore-water support, rerun landing;
then missing island population/region ownership, not asset polish. Allfour travel,
allfive complete life/families, unrestricted connected saves/FPS remain gaps.
No worker remains assigned or native app running. Earlier accepted NPCdiet is
published e73f6a1; this local checkpoint does not replace its accepted evidence.

'''
for f in ['current-work.md','DRAFT_COVERAGE.md','GENERAL_PROJECT_STATE.md']:
 p=r/'.clanker'/f;old=p.read_bytes();prefix=notice.encode('utf-8');p.write_bytes(old if old.startswith(prefix) else prefix+old)
print('PARTIAL_CHECKPOINT_WRITTEN',failure)
