# Observable hero life-stage working draft

The existing calendar Lifecycle now drives a connected-world stage cue, a brief
milestone and one tunable horizontal body-bulk change at adulthood. No second
lifecycle or renderer-only stage machine exists. Ordinary starting age remains0;
Hatchling and Juvenile both retain starting bulk1. Adult/Old/Dead use proposed
bulk1.25. The original saved/nonconnected presentation always retains bulk1.
No clocks, durations, saved ages, save formats, needs rates or motion pace change.

Tune domain/life_stage_presentation.hpp's five-value horizontalBulk array. This is
a replaceable display approximation: the current juvenile model becomes broader
and longer at adulthood, not a new anatomical adult asset. Vertical scale and
sole offset stay unchanged, and the existing support footprint follows horizontal
bulk. The existing root/stride locomotion speed remains unchanged. No source mesh,
rig, animation or immutable asset is modified. Other stages have a cue but no
distinct anatomical forms. Current normal hatchling/model age mismatch remains
honest, rather than silently changing starting age to fit the available model.

Use scripts/Run-ConnectedWorld.ps1 -Interactive for ordinary play. A new default
hero reaches adulthood at existing calendar age3000, with the normal clock.
-LifeStageProbe -Duration18 starts only the diagnostic hero at age2994 and fatigue
.8, previews noon and injects ordinary reverse input at physical seconds9..11.
Add -Proxy for the proxy trial; the default imports the existing animated body,
fern and tree assets. Screen/JSON labels identify synthetic startup/fatigue/input/
lighting. Clock scale is not modified. Six actual calendar seconds cross the real
threshold; this is not a lighting phase being called growth. Connected save/load
remains refused. Original mode's startup/save code is unchanged.

The composed captures are life-stage-0..3.png: stationary juvenile, stationary
adult, post-transition direct movement and ordinary activity afterward. life-stage
JSON includes actual ages/clocks/milestones, applied transform, conservative native
renderer bounds and needs/control samples. Proxy native width ratio1.24998 and
other horizontal ratio1.24417; animated bounds ratios1.09875/1.04173 also depend on
pose and conservative component bounds, so do not treat them as anatomy measures.
Both show the broader body. One adulthood event at age3000; rest recovery remains
.05 per physical second across it, thirst.0002 per calendar second. Afterward the
proxy moves4.0036m and animated body.32234m under existing reverse controls. All
final probes have zero support failures/fallbacks. Existing rest hysteresis applies
on release: fatigue is below entry threshold, so rest does not automatically resume.

Parked: abrupt horizontal-only change, shared juvenile anatomy/old-stage stand-in,
existing gait stride/sole/foot-locking/contact/wetness limits, and generic later-stage
milestone wording. This proves one player-facing transition, not complete growth,
physiology, family, anatomy, human input, finished visuals or performance. Working
draft -> log these limits -> next subsystem. Exact native evidence/tests/baseline
and root-only review provenance are in the task receipt.
