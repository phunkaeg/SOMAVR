# Runtime Analysis: 0.5.1 Compatibility Probe

Source log: `logs\somavr.log`, captured 2026-07-12 from `0.5.1-compatprobe`.

## Result

The run confirms the complete experimental path:

- F8 created and ran the OpenXR session.
- F10 applied native HPL3 HMD orientation.
- F11 produced user-confirmed stereoscopic vision with runtime eye separation and asymmetric FOV.
- All seven compatibility hooks installed and executed without a signature, capture, frame, or suspension failure.
- Visual shader defects remain, but no GLSL compile/link error was logged; they are render-policy or eye-history defects rather than failed shader creation.

## Timeline

| Event | Game frame | Time |
| --- | ---: | --- |
| OpenXR manual start, F8 | `2296` | `15:13:05` |
| Native HMD tracking, F10 | `2438` | `15:13:08` |
| AFR stereo enabled, F11 | `2890` | `15:13:15` |
| AFR disabled/re-enabled, F11 A/B | `3490` | `15:13:25` |
| Last periodic OpenXR row | `4695` | `15:13:46` |

The last OpenXR summary reported a focused session, `openxrFailed=0`,
`openxrFrameSubmitFailed=0`, `stereo=1`, `1767` captured eyes, `1765` submitted
stereo frames, and `2383` total submitted OpenXR frames. Eye-cache capture later
reached `1800` without failure.

## Native Render Split

The gameplay viewport was stable at `0x0B812D10`, with render mask
`0xffffffff`. The relevant mask bits remain `1` world, `2` screen GUI, and `4`
post effects.

Observed active-gameplay order:

```text
viewport begin on FBO 0
  world: FBO 0 -> FBO 11
  world overlays: FBO 11 -> FBO 11
  post effects: FBO 11 -> FBO 0
  PostPostEffects phase (then labeled callbacks): FBO 0 -> FBO 0
  screen GUI: FBO 0 -> FBO 0
viewport end on FBO 0
```

Transition counts across sampled rows:

| Stage transition | Samples |
| --- | ---: |
| Screen GUI `0 -> 0` | `41` |
| World `0 -> 11` | `31` |
| Post effects `11 -> 0` | `29` |
| Post effects `0 -> 0` in non-world/menu states | `12` |

Average sampled cost after F11:

| Stage | Average | Maximum |
| --- | ---: | ---: |
| World | `1530.1 us` | `2418.7 us` |
| Post effects | `125.4 us` | `232.4 us` |
| World overlays | `0.4 us` | `1.2 us` |
| PostPostEffects phase (then labeled callbacks) | `0.1 us` | `0.3 us` |
| Screen GUI | `24.7 us` | `79.1 us` |
| Whole viewport | `1783.3 us` | `2795.4 us` |

This proves a practical post-effect isolation boundary. It does not yet prove that
world rendering can be invoked twice safely because pre/post-world callbacks remain
inside the current viewport coordinator.

## Audio Listener Ownership

Hypothesis S11 is confirmed. Under F10/F11, the HMD quaternion changed strongly
while listener forward/up remained constant for long intervals. Examples:

- frames `2583-3060`: listener forward stayed near
  `0.22721,0.20276,0.95250` while HMD yaw changed across both signs;
- frames `3899-4499`: listener forward stayed
  `0.88992,0.15906,-0.42749` through large HMD quaternion changes;
- listener vectors changed only when SOMA's authored camera/state changed.

The current frustum bridge therefore does not make FMOD head-relative. The safe
correction is to rotate the current authored listener forward/up by the physical
HMD delta around the F10 neutral pose, commit those temporary vectors to FMOD, and
restore SOMA's fields immediately. Position and velocity should remain authored in
the first implementation.

## Shader Defects

The log contains no shader compilation failure or OpenGL/OpenXR error. Candidate
causes are therefore:

1. temporal matrices/history alternating between eyes;
2. image-trail or another history-based post effect sharing one buffer;
3. screen-space effects using desktop assumptions;
4. a world shader whose view-dependent state is not fully rebuilt per AFR eye.

`HPL3_PostEffectComposite_HasActiveEffects` is the lowest-risk A/B boundary. A
runtime bypass can return false, allowing SOMA to render directly without entering
the composite chain. If the defects disappear, individual effects can then be
classified. If they remain, investigation moves into world shader camera packets.

## 0.5.2 Decisions

- Preserve the proven F8/F10/F11 and AFR cache paths unchanged.
- Apply orientation-only center-head audio correction while F10 is active.
- Add F12 as a reversible all-post-effect bypass, default off.
- Keep HUD and screen GUI rendering active during bypass.
- Retain compatibility telemetry to compare FBO/call behavior with F12 off/on.
- Defer same-frame dual rendering until callback and temporal ownership are narrower.
