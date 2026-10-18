# Smalltrotter exact seam independent review, 2026-10-17

Decision: accept candidate seam algorithm for coordinator integration; base cage
preparation only. Root integration and production replacement were not performed.

Clean candidate branch: codex/agy-smalltrotter-exact-seam-repair
Candidate commits in order:
- 22e3b0a33337a0d3f4aebfa7ac5e880cdc0d3f45
- 0d7ff4f (review gates)

Changed files are limited to:
- stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py
- stalthos-runtime/docs/smalltrotter-source-preparation.md

Independent executions (all completed with exit 0):
1. python -O var/worktrees/agy-smalltrotter-exact-seam-repair/stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py --selftest
2. "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --disable-autoexec --python var/worktrees/agy-smalltrotter-exact-seam-repair/stalthos-runtime/scripts/assets/prepare_smalltrotter_source.py -- --mode seam-preparation --input assets/source/creatures/smalltrotter/smalltrotter-rough-master.blend --output var/seam-review-20261017/derivative.blend --receipt var/seam-review-20261017/seam.json
3. Blender background render using render_review.py in this task directory.
4. git diff --check in candidate worktree.

Selftest: 9 active checks passed under optimized Python. Native receipt:
824 mutual nearest pairs; 0 ambiguous; vertices 34144 -> 33320; edges 67460 ->
66636; faces preserved 33318; components 2 -> 1; boundaries 1648 -> 0;
over-connected edges remain 0; bounds exactly preserved. Other mesh fingerprints
verified after reopening. Reopened body topology and Multires absence verified.

Master before/after SHA-256:
B987E7D7EAC4DFE0E09E2E188A592236ED57EB5E00DA7D8F7E6D1D901DE15350
Derivative SHA-256:
021A0E773FA13EDFCE4B0F32C00EF13B34281E7A94BB2B6AE57C3E92C774627B

Actual Blender stdout/stderr: var/seam-review-20261017/seam.log
Structured receipt: var/seam-review-20261017/seam.json
Selftest stdout: this directory/selftest.json
Captures: var/seam-review-20261017/{three-quarter,front,side}.png
Front and three-quarter independently inspected. No obvious open center split;
rough four-legged long-necked silhouette coherent. Workbench captures cannot
accept final surface treatment, anatomy, UVs, rigging, animation, runtime or FPS.
Master untouched. Multires intentionally absent from derivative base cage;
original master retains sculpt detail. Historical invalid derivatives untouched.

Review fix: previous candidate reported closed topology in evidence but did not
enforce closure, exact vertex loss or reopened body topology before success.
Now those are active failure gates. No toolkit code changed, so no toolkit suite
rerun was required for this isolated runtime utility review.
