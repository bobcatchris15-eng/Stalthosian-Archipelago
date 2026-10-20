# Ordinary rest and recovery working draft

Connected routine play now uses the existing NeedsCore Rest suggestion. The
uncommanded tired hero stays in the existing idle animation, shows a small
Resting - recovering energy cue, and recovers fatigue through the existing
NeedsCore physical-time rate. The rest controller advances no clock and mutates
no resource ledger. Direct movement or E stops rest and its recovery immediately.
Rest continues below the entry threshold until fatigue reaches0.3, avoiding
rapid switching. A more urgent food/water need leaves rest and returns to the
same existing drink/browse controller. Original nonconnected saved-mode rest
flags are untouched; there are no new save fields.

The cue uses the pinned engine SpriteFont/AddFont interface, with logical-canvas
center/bottom anchoring and safe inset. Visibility changes only on activity
transitions. Native composed-backbuffer captures include the actual cue. Only
1920x1080 native output is verified; no broad UI/device acceptance is claimed.

Run.ps1 -HeroRestProbe -Duration20 starts with explicit synthetic fatigue0.8.
-HeroRestOverrideProbe adds synthetic reverse input during wall-time4-6s.
CalendarScale500 with Plant tests a higher thirst need taking over after rest.
Neither fixture teleports the body after startup. The main rest proof reuses the
existing reviewed animated Smalltrotter; override/handoff use the ordinary proxy
integrator. No physical keyboard trial is claimed.

Focused tests cover once-only physical recovery, calendar-rate separation,
hysteresis, immediate direct override, higher food priority and terminal stop.
Native tests cover stationary body/visible cue/recovery, direct movement and
resume, and rest-to-drink handoff. No dedicated detached-camera rest trial was
added: rest has no camera input, and existing camera tests remain. Resource
contacts remain solely in the existing food/water interaction pipeline.

Limitations: idle standing is the draft rest pose, not authored sleeping/crouching;
no shelter choice, nest/resting place preference, species-specific rest or sleep
cycle yet. No new anatomy/assets, original saves changed, generalized AI or FPS
acceptance. Working role -> log oddities -> next subsystem: environmental exposure.
