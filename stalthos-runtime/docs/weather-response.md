# Weather and local exposure working draft

Normal connected play uses the existing deterministic environment sampler and
calendar clock. It now drives overcast lighting (sun strength, sky exposure,
ambient and fog) and wind. Rainfall remains a sampled input to body wetting;
the pinned native rain emitter is disabled after a observed transition crash. The existing connected grass
material uses wind weights. This maps cloud cover to lighting; no volumetric
cloud geometry or new climate solver is introduced.

```powershell
.\stalthos-runtime\scripts\Run.ps1 -ConnectedWorld -Interactive -CameraDistance 10
```

Body wetness accumulates from rain in **physical time**, making the proxy body
darker and smoother. It dries slowly in dry conditions. The coarse lee of the
visible route outcrop groups reduces a local wind/comfort proxy. Wind shelter
never blocks rain: it is not a waterproof roof or physical occlusion solver.
Normal shelter comes from the actual body position relative to those outcrops.
This first draft does not couple comfort to needs, damage, lifecycle or movement.
The local wind/comfort response is currently in telemetry; visible body wetting
is the player-facing consequence. No HUD is added.

For a short, explicitly synthetic visual demonstration:

```powershell
.\stalthos-runtime\scripts\Run.ps1 -WeatherPreview -Duration 12 -CameraDistance 10
```

The three equal diagnostic phases are clear, exposed rain, then continued rain
with synthetic local wind shelter. These override only weather inputs; movement,
calendar and lifecycle clocks retain their existing values/rates. The synthetic
shelter phase is not proof of player navigation to shelter or physical occlusion.
The diagnostic produces three weather-response screenshots and quarter-second
samples in weather-response.json with clocks, actual body position, rainfall,
wetness, exposed/local wind, comfort proxy and synthetic-phase labels.

`domain/weather_response.hpp` owns the small wetting/drying/wind reduction tuning
and coarse outcrop lee model. The existing ConnectedRouteLayout supplies landmark
positions. Changes remain limited to the connected preview: original populated
and diagnostic modes preserve their earlier weather path, and connected preview
still explicitly refuses save/load.

Draft oddities to log and move past: lighting approximates overcast rather than
rendering authored clouds, no visible rainfall effect is claimed, grass
wind weights are approximate, weather changes naturally on the existing long
weather cycle, the body is a proxy, and shelter/comfort lacks a player HUD or
needs consequence. No final effects, thermal physiology, waterproof shelter,
persistent weather state or target-hardware performance acceptance is claimed.
