# Environmental exposure working draft

Connected preview only. Existing body-local weather comfort drives additional
fatigue when comfort falls below 0.5. Proposed rate: 0.012 per physical second
at severity1, linearly reduced toward threshold. Added fatigue stops at 0.65;
higher preexisting fatigue remains intact. No injury/lifecycle effect. Existing
NeedsCore.advance remains the sole normal recovery driver; exposure is applied
once afterward. Normal dry mild weather incurs zero additional fatigue.

Existing wetness and local wind determine comfort. Temperature is not simulated
by this approximation. Coarse outcrop lee reduces wind, never rain interception
or wetness. A centered amber cue says weather exposure is tiring the body faster.
The cue switches to exposure eased in a partial lee; fatigue can still accumulate.
Controls remain authoritative. Camera position is never read by this rule.
Tuning is the small ExposureTuning struct in domain/environmental_exposure.hpp.

Run.ps1 -ExposureProbe -Duration26 uses explicitly synthetic adverse weather,
an initial fixture near the first existing rock cluster, and the ordinary forward
intent/integrator from physical second11 to12. Proxy pace is the existing2m/s;
this is not actual-animal crossing/contact acceptance. No post-start teleport,
synthetic shelter override, rain-art or roof-occlusion claim. Composed native cue
captures and environmental-exposure.json record the reaction and shelter change.

Known inherited limits: coarse lee geometry; no collision-derived occlusion;
proxy wet-material tint only; native rain emitter disabled; exterior animal sole
contact unresolved. Working saves retain prior contracts; connected preview
still refuses save/load. This rule adds no save format.
