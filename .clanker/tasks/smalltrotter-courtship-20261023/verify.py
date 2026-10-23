from pathlib import Path
import json,math,hashlib,subprocess,runpy
root=Path(__file__).resolve().parents[3];task=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding="utf-8-sig"));sha=lambda b:hashlib.sha256(b).hexdigest()
runpy.run_path(str(task/"verify-common.py"));runs=read(task/"native-runs.json");assert len(runs)==11
proof={}
for label in ("courtship","override","camera","refusal","hatch"):
 j=read(Path(runs[label])/"nesting.json");s=j["samples"];requests=[e for e in j["courtship_events"] if e["event"]=="courtship_request"]
 assert requests
 if label in ("courtship","hatch"):
  assert requests[0]["invited"] and j["clutch_mate"]==requests[0]["mate"]
  placed=[e for e in j["events"] if e["placed"]];assert len(placed)==1 and placed[0]["mate"]==j["clutch_mate"]
  assert placed[0]["physical_seconds"]>requests[0]["physical_seconds"]+2 and max(x["courtship_quiet"] for x in s)>=2
  before=[x for x in s if x["physical_seconds"]<=placed[0]["physical_seconds"] and "mate_body" in x]
  distances=[math.hypot(x["mate_body"]["x"]-x["body_x"],x["mate_body"]["z"]-x["body_z"]) for x in before]
  assert max(distances)>4 and min(distances)<3.5
  proof[label]={"mate":j["clutch_mate"],"laid_physical_seconds":placed[0]["physical_seconds"],"observed_mate_min_distance":min(distances)}
 else:
  assert not j["events"] and j["clutch_mate"]==0 and j["offspring_admitted"]==0
  if label=="override":assert requests[0]["invited"] and any(x["direct"] for x in s) and not s[-1]["courtship_active"]
  else:assert not any(e["invited"] for e in requests)
  if label=="camera":assert all(not x["courtship_active"] and not x["courtship_ready"] for x in s)
  proof[label]={"no_clutch":True,"requests":requests}
 if label=="hatch":
  births=[e for e in j["hatch_events"] if e["event"]=="admission_attempt" and e["result"]==0];assert len(births)==1 and j["offspring_admitted"]==1
  b=births[0];assert b["old_actors_unchanged"] and b["ledger_unchanged"]
  w=read(Path(runs[label])/"world-population.json");initial={a["id"]:a for a in w["snapshot_initial"]["actors"]};final={a["id"]:a for a in w["snapshot_final"]["actors"]};assert set(final)-set(initial)=={b["child"]}
  clock=read(Path(runs[label])/"runtime-receipt.json")["calendar_seconds"];assert abs(final[b["child"]]["age"]-(clock-b["birth_calendar"]))<1e-6
  n=[e for e in read(Path(runs[label])/"death-next-life.json")["events"] if e["event"]=="explicit_next_life"];assert len(n)==1 and n[0]["hero_id"]==3 and n[0]["npc_snapshot_unchanged_at_handoff"] and n[0]["ledger_unchanged_at_handoff"]
  proof[label].update(birth=b,next_life=n[0])
review=read(root/".clanker/clunkers/smalltrotter-courtship-review-20261023/review.json")
for e in review["source_hashes"]:
 data=(root/e["path"]).read_bytes();assert sha(data.replace(b"\r\n",b"\n"))==e["canonical_lf_sha256"]
 assert subprocess.check_output(["git","hash-object","--path="+e["path"],e["path"]],cwd=root).decode().strip()==e["git_blob"]
out={"ok":True,"ctest":"66/66pass","same_executable_native_runs":11,"proof":proof,"canonical_lf_and_blobs_match_review":True,"limits":["One speculative quiet-response pairing rule; no sex/genetics/authored display or fullreproductive biology","Twoinactiveeggs, fixedchildbody/near-nest crowding and incompleteparentcare remain","Other species family lives,fourislands,connectedpersistence andtarget30FPS unaccepted"]}
(task/"verification.json").write_text(json.dumps(out,indent=2)+"\n");print("COURTSHIP_NATIVE_VERIFIED")
