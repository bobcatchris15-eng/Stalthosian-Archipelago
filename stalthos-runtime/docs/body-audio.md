# Native body-motion rustle: working draft

Connected mode now plays one quiet generated mono ground-rustle cue from actual
living supported hero displacement. Proposed tunables: .06m accumulated motion,
.45physical-second minimum interval,1m maximum accepted frame step, .12 instance
gain and24m simple camera-distance attenuation. Unsupported/dead/stationary bodies
and teleports do not emit. New life resets cadence and pauses the prior sound.
Camera motion changes listening gain, never body emission or HeroSim knowledge.
Original saved mode remains unchanged. No global/submix/user volume/routing changes.

bodyRustleWave produces original deterministic22050Hz mono16-bit PCM noise with
a.14second envelope, a replaceable stand-in rather than a recording or measured
foot contact. One Wicked Sound and SoundInstance are reused, with bounded source
cursor/event evidence (256 detailed events plus bounded counters). No voices, panning, reverb, occlusion, surface variety or
sound-art acceptance. Existing needs/motion/calendar ownership is unchanged.

BodyAudioProbe labels ordinary forward2..6 and reverse8..10 input. Duration14
records its own native output. BodyAudioCameraProbe -Duration12 pans only the
camera while body is stationary. Diagnostic capture first attempts Windows
process-loopback INCLUDE_TARGET_PROCESS_TREE for the current PID. If unavailable,
the guarded default-endpoint fallback refuses/discards capture while any other
audio session is active. It never changes another session or user settings.
The successful evidence uses actual process isolation, excluding unrelated apps.

Reference API: https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ns-audioclientactivationparams-audioclient_process_loopback_params
Official sample: https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/
The documented baseline does not replace capability probing: this host reports
Windows10build19045, yet actual process activation/output succeeds with HRESULT0.
No claim about why that capability is present is made.

Native44100Hz stereo16-bit output contains12 bursts at about.45s spacing; motion
RMS.00210/peak.02469 versus camera-only dither RMS.0000146, zero body events/voice
progress. Stationary windows are dither-only. Native death/new-life probe emits
only for fresh hero2 at physical12..14, never death or spawn. Output is actual
native digital mix, not physical-speaker/human listening acceptance. Captured
samples omit startup/frame-stall wall-clock gaps; no sample-exact latency or
continuous capture claim. The generated source WAV alone is not output evidence.
Working draft -> park sound-art/spatial/contact/capture-timing limits -> next.
