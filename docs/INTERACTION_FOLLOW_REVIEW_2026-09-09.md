# Drawer, Door And Throw Follow-Through

Date: 2026-09-09. Build: `0.96.1-interaction-follow`.
Owners: `FEATURE.PHYSICAL_MANIPULATION`, `HPLGrabBridge`, `HPLGrabMath`,
`HPLInputBridge`. No game launch or new native call/address in this review.

## Evidence

Question: does manipulation lose response because the hand leaves a hard axis
threshold, because the motion target changes, or because native physics resists?

The tested 0.96.0 log is retained under
`logs/receipts/2026-09-09-interaction-follow/baseline-logs/somavr.log`.
`baseline-sha256.json` identifies that log and the installed Grab/Slide/Door
scripts. These are SOMA's shipped scripts, not an HPL2 ancestor inference.

- No hard angular acceptance threshold exists in the direct Slide/Door paths.
  Velocity is projected onto the joint pin or handle tangent, so perpendicular
  motion still contributes less. Native joint limits/friction remain in charge.
- Slide frame 12619: hand displacement 0.1461, body displacement 0.1579,
  along-pin hand velocity 0.4021. Velocity gain 1.25 boosts initial response, but
  position feedback used unscaled travel and braked it to target speed 0.2902.
  A persistent sensitivity boost cannot survive that conflicting position target.
- Door used initial hit point plus controller displacement as a virtual handle.
  Its hinge radius could grow with the pull, reducing angular velocity through
  the `cross(radius, velocity) / radiusSquared` calculation. This is a source
  defect consistent with the report, not yet a headset-proven root cause.
- Trigger release only sent left-up, entering `Grab::DropBody`; no native throw
  action/redirect rows occur in this log, while grab/joint hooks are active.
  `Grab::PostUpdate` drops when neither Interact nor InteractRotate is held.
  The A action previously released Interact before queuing Throw, a race with
  that drop check.
- Native Grab hold limits linear speed to 2 (normal) / 1 (heavy); DropBody caps
  exit speed and calls `EnableCharCollisionUntilStopped`. Throw instead restores
  original body properties, zeros velocity, and adds its native impulse.
  `ResetPropVars` requests collision suppression until outside the player.
  This makes the release route a concrete suspect for weak throws/recoil;
  collision ordering and actual player overlap still require runtime evidence.
- Correction to the historical 0.68 note: the current Throw script constructs
  a camera transform but never applies it with `SetMatrix`. There is no evidence
  here of a throw-time teleport. A forward direction cone is not a collision test.

## Implementation

- `SlideVelocityScale` now scales both desired displacement and feed-forward
  velocity. The package retains 1.25. Existing position gain and speed cap remain.
  Linear velocity now uses the same world-unit conversion as position/door motion.
  A valid tracked position can seed an anchor even without runtime velocity;
  position-only feedback/finite differences no longer deadlock before acquisition.
- Door/lever hit point is attached to the grabbed body's initial rigid pose and
  transformed using its current validated matrix. The actual handle rotates with
  the door; hand travel no longer changes its radius artificially. Joint pin,
  pivot, PID and collision/limit behavior are unchanged. Pure wrist-driven lever
  motion remains available even when linear velocity is unavailable.
- Fast trigger release (tracked velocity at least max(0.8 m/s, configured throw
  threshold), without the rotate grip held) requests the existing native Throw.
  Slow release remains native placement/drop. The redirect must bind to the exact
  live grab body/player; unrelated impulses cannot consume it. A retains native
  action fallback when a redirect cannot be armed.
- Hold native Throw and Interact until state exit, at most 250 ms. This prevents
  PostUpdate from dropping before it receives the action. Stale repeated input
  cannot extend the timeout or repeat the same A action. Pause/tracking loss
  clears the handoff and redirect. No new collision writes or mass/velocity calls.
- Direction/magnitude still use the established native-impulse policy, not a
  claimed exact 1:1 ballistic release: native strength to 2x, with forward cone.
  Arbitrary rearward throws and verified no-player-contact release remain open.

## Diagnostics And Acceptance

Joint logs now include a sample every 250 ms per active grab in addition to the
existing early/periodic samples. `hpl_slide_target` includes velocity source,
scaled position error, target speed, native error and full velocity/pin vectors.
`hpl_rotate_target` includes the real handle point, pivot and target angular speed.
These distinguish projection loss from feedback and native resistance offline.
Throw rows identify `native_drop` versus `fast_trigger_release`/`primary_button`,
the exact armed body, actual redirected impulse, state exit and timeout.

Controlled test, same save and same objects, no simultaneous stick locomotion:

1. Drawer: one slow continuous open/close. Repeat with a mildly diagonal pull.
   Hold still halfway, then reverse. Success: no increasing required hand travel,
   no runaway while still, retained stops. Curtains are the regression control.
2. Door: one continuous open/close while following a loose arc. Reverse halfway;
   compare early/late response. If it still stalls, preserve this log before
   changing gains; tangent alignment and native resisting error decide the next step.
3. Light physics prop: place gently, then throw with trigger release while the
   hand is moving; compare with A. Test both hands, and note any player displacement.
   Success needs an applied redirect plus useful distance/no recoil, not merely
   a requested action. A timeout or fast release falling back to Drop is actionable.
4. Existing neutral-wrist (-45 pitch, -90 roll) and Read (1.2 distance, 2x size)
   tuning remains included, as do Ctrl+F10 arm/depth diagnostics.

Verification: Release build and all ten CTests pass, including constant scaled
slide response, the recorded drawer sample, rigid handle rotation/translation,
degenerate/radial hinge motion, release thresholds, handoff completion/timeout,
and native impulse bounds. No live physics or headset acceptance is claimed.
