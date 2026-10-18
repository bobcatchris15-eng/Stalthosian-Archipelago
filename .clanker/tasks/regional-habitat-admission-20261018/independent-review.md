# Independent review

Two isolated native agents implemented nonoverlapping ledger and habitat packets.
Each packet was source-reviewed independently by the other agent or root before
publication; root ran integrated Release, domain and native checks.

Findings resolved:
- Population previously relieved needs before ledger acceptance. Host now disables
  local relief and applies only accepted contact relief.
- Ledger Replay previously counted as fresh adapter consumption in root candidate.
  Only NewKey/Update emit events or relief; unhydrated replay test added.
- Native probe used separate horizontal/vertical browse thresholds and horizontal
  water adjacency. Tightened to full3D .8m browse, water .8m reach plus .45m vertical
  separation. Independent source re-review confirmed fixes.

No unresolved source blocker reported. Limits: sampled route gaps below interval,
cardinal freshwater search, approximate draft contact anatomy, bounded full-history
starvation of new pairs, no regional registry/spawn. Native callback evidence is
required separately from synthetic unit tests.
