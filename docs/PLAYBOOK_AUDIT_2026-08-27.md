# VR Modding Playbook Audit - 2026-08-27

Status: audit complete; no runtime behavior changed.

## Scope

Compared the `0.94.0-afr-pair-coherence` baseline against the updated cross-engine
playbook, especially:

- `pattern-catalog.md`: `META-005/006`, `STR-002..006`, `HAND-005/006`,
  `PERF-001..005`, and `TEST-001/002`;
- chapters 02, 09, 10, 11, 18, and 19;
- the generated fleet bottleneck map dated 2026-08-27.

The fleet map correctly places SOMAVR at T3 refinement. Its active gates are
`BN-RND-001` and `BN-PERF-001`: per-eye OpenGL resource correctness and the
fresh-frame budget. Camera discovery, XR transport, pair coherence, input, and
packaging are not the current bottleneck.

## Actionable Findings

### P1 - Replace the projected fixed-direction elbow pole

Evidence: `[STATIC]`.

`HPLArmIKMath::ComputeErgonomicElbowPole` currently constructs a preferred
down/out/back direction and calls `ProjectDirection(preferred, armAxis)`. This
is exactly the construction retired by playbook `HAND-006`: it is degenerate
when the arm axis aligns with either the preferred direction or its antipode.
The existing torso-local history blend and swivel-rate limit soften motion near
one weak region, but they cannot remove the second singularity.

Next implementation should:

1. Build the ordinary pole perpendicular by construction from the arm axis and
   torso side axis using a cross product.
2. Use the cross-product magnitude as the continuous proximity signal for the
   remaining lateral-arm singularity.
3. Blend into the existing torso-local history/native fallback before the
   current spherical swivel limit; do not introduce a hard threshold.
4. Preserve independent left/right state and every current non-finite fallback.
5. Add pure-math sweeps through rest, vertical, behind-shoulder, the old
   antipode, and the new lateral degeneracy before a headset build.

This is high-confidence math work, but final elbow feel and handedness still
need headset acceptance.

### P1 - Publish a fresh-frame ledger, not only timing averages

Evidence: `[STATIC]`.

SOMAVR already records nearly every ingredient required by playbook `PERF-001`:
XR wait time, pair capture delta, cache age, stereo submissions, held-pair
episodes/frames, focus-pacing skips, GL transfer CPU/GPU samples, replay draw
count, and stage timings. They are distributed across several periodic and
shutdown rows, so a run can still report healthy average timing while hiding a
poor fresh-pair rate.

Add one `openxr_freshness_summary` row with explicit counts and ratios for:

- running XR frames and `shouldRender` frames;
- newly completed coherent stereo pairs;
- fresh-pair submissions;
- held-pair submissions;
- black/fallback projection submissions;
- focus/unfocused skips and failed/incomplete pairs;
- latest/maximum pair age and effective fresh-pair rate.

The row must label unavailable measurements rather than printing a plausible
zero. It should complement, not replace, the existing detailed timing rows.

### P1 - Attribute pacing to Wait, Begin, or End before moving threads

Evidence: `[STATIC]` plus cross-project `[LIVE]` corroboration.

`xrWaitFrame` remains on SOMA's `SwapBuffers` thread and SOMAVR records its
duration. It does not yet publish comparable `xrBeginFrame` and `xrEndFrame`
durations. Playbook `PERF-003` shows that a half-rate pipeline can move the
blocking time into `xrBeginFrame`; moving Wait to a worker can stop a hard
wedge while leaving the game thread paced by the handoff.

Instrument all three calls independently before designing a wait-ahead worker.
Only adopt the worker/permit ordering if a matched headset run shows the
dominant delay and proves the new ordering moves it into Wait without reducing
fresh application cadence. The current direct-wait risk remains open.

### P2 - Consider a substitute runtime as a development instrument

Evidence: playbook `TEST-001`; applicability is `[INFERENCE]`.

SOMAVR's long headset feedback loop makes a per-process substitute OpenXR
runtime potentially valuable for frame-loop, layer-budget, FOV/pose-tag, and
recovery-scenario tests. It would not prove headset optics, runtime focus
negotiation, controller profiles, or vendor behavior. This is worthwhile only
after the real-runtime freshness ledger exists, so the instrument can model a
small, explicit API surface rather than becoming a second product by accident.

### P3 - Keep engine-native IK discovery as a bounded question

Evidence: `[STATIC]` string census, not a proof of absence.

Playbook `HAND-005` says to enumerate a target's own named two-bone facilities
before maintaining a custom solver. A calibrated literal-string census of the
supported executable recovered the known-positive `cCamera` and `SetMatrix`
controls, plus Tobii `LeftEye`/`RightEye` vocabulary, but no `IKLimb`,
`InverseKinematic`, `TwoBone`, `IKSolver`, `PoseModifier`, `SolveIK`, or
`EffIK` family. This proves absence of those names, not absence of an unnamed
solver. Existing HPL2-backed native-stereo work also proves there is no
Anvil-style camera override pointer and that the useful engine-owned seam is
the explicit frustum parameter.

Do one bounded Ghidra/source-oracle search for a limb solver or pose modifier
when visible-hands work is next scheduled. A negative result must include a
known-positive search control. It does not block the cross-product correction.

## Already Aligned

- `STR-002`: version 0.94 publishes only a coherent pair and latches the native
  base plus complete OpenXR stereo snapshot across AFR eyes.
- `META-005`: pair state has a 100 ms expiry; input, interaction ownership,
  grab, hands, flashlight, and contact-haptic lanes already carry bounded
  stale-data rejection with safe inactive fallbacks.
- `META-006`: `NATIVE_STEREO_FEASIBILITY.md` already asks the engine-interface
  question. HPL supplies the frustum as an explicit render parameter; no usable
  hidden stereo device or nullable camera override has been found.
- `STR-003/004`: projection companions, per-eye temporal owners, caller-RVA
  camera classification, viewport provenance, and secondary-view exclusions
  are explicit. The unresolved translucent camera UBO/refraction scratch is
  correctly still an active render-correctness gate.
- `XR-004`: F10-off now destroys session and instance and permits a clean F10
  restart.
- `TEST-002`: OpenGL observers report hook/call coverage and overflow/failure
  counters; GPU timers distinguish unavailable, dropped, and invalid samples.
- `PERF-004`: optional foveation and HUD-cylinder extensions are negotiated,
  function-pointer guarded, result checked, and fail back to native paths.
- `STR-006`: AFR and same-frame replay remain independently selectable and
  shippable. A larger policy-interface refactor is justified only if native
  stereo or depth reprojection becomes a real third rung.

## Not Applicable Now

- Managed-engine, companion-plugin, source-port, multiplayer, and D3D12 queue
  guidance in chapters 18/19 does not change SOMAVR's native HPL3/OpenGL route.
- Presentation-scale-as-FOV applies to submitting an already-upscaled image at
  a smaller angular size; it is not a replacement for SOMAVR's eye render-scale
  and swapchain-resolution controls.
- Publishing a public extension API is release-ecosystem work, not a current
  render-correctness or performance gate.

## Recommended Order

1. Add Begin/End timing and the fresh-frame ledger without changing rendering.
2. Implement and unit-test the cross-product elbow pole as a separate bounded
   behavior change.
3. Run the existing matched headset scene and collect pair, stage, GL transfer,
   query/refraction, and freshness evidence.
4. Move `xrWaitFrame` only if the call-duration evidence justifies the exact
   wait-ahead permit design.
5. Promote native stereo only if draw/GPU cost and sequential per-eye resource
   ownership both pass.
