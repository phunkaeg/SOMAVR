# PreyVR Arm IK: Transfer Assessment

Date: 2026-09-09. Original static source and saved-headset-evidence review made
no game attach, native mutation, DLL rebuild, or headset acceptance.
Features: `FEATURE.VISIBLE_HANDS`, `FEATURE.ARM_GOAL_CONSISTENCY`.

## Implementation Follow-up: 0.96.0

`HPLHandsMath::BuildCalibratedWristGoal` now builds one desired endpoint from the
tracking frame's camera and per-hand pose. Arm IK consumes that endpoint; after
successful upper-arm/forearm writes, `CommitWristIKGoal` publishes the solver's
reachable wrist endpoint to the position/orientation writer. Wrist writes no
longer add calibration a second time or undo the arm solver's reach clamp.
IK-off/failure keeps the calibrated requested endpoint. Existing per-frame and
entity guards remain; no native addresses, twist solver or rotation knobs added.

Pure tests cover body headings, mirrored outward offsets, world scale, reachable
1:1 targets, unreachable clamped targets and invalid inputs. All 10 CTests pass.
`hpl_wrist_goal` logs requested/selected/actual positions, clamp and residual in
meters, making a wrongly located shoulder distinguishable from wrist rewriting.
No headset acceptance yet: wrist twist, shoulder fit and per-eye animation lag
remain separate questions. See `NEXT_LIVE_EVIDENCE.md` for the controlled test.

The remaining source references describe the pre-implementation 0.95.9 conflict.

## Is Prey's Solution Working Well?

It has useful native position/rotation takeover, but is **not an accepted final
arm solution**. Its latest detailed headset record (September 8) reports:

- Moving the right controller appears to influence the left mesh. The original
  isolation test was confounded; shared solver/target ownership is unproved.
- `ikClamped=7068`, `ikWrittenR=14407`, approximately 49%. Treat this as the
  reported comparison, not a hand-normalized rate measured here. Visible clicks
  could be clamps or rejected writes; elbow flipping remains a hypothesis.
- Raising reach to 125 worsened placement. The control scales controller travel;
  lowering it compresses travel and sacrifices 1:1, not a real limb-length fix.
- Rotation could be lost after re-equip/recenter. The subsequent audit reproduced
  lost automatic recovery across repeated `Bind` calls and fixed preservation
  of `calibrated | autoPending`. Regression tests failed before/passed after;
  headset recovery quality has not been established by that offline result.
- Default `ik.hands=1` drives only the right hand. The left-hand absence was
  partly a control policy, not absence of a native left chain.

Source: `D:/Dev Debug/PreyVR/docs/HANDOVER-HEADSET-SESSION-2026-09-08.md:212`,
with corrections in `RE-HEADSET-SESSION-AUDIT-2026-09-08.md:127` and section 6.
Inspected Prey HEAD `bd7f28179db6eb301525f9431d782ae17809106e` (2026-09-08).
Do not promote the older handover's stronger causal guesses over its later audit.

## What Is Worth Borrowing

Prey hooks CryEngine's animation-driven IK before the native solve and before
weapon attachments consume its result. It supplies one absolute model-space
position/orientation target, sets the native weight, and lets native two-bone
IK and wrist/descendant propagation operate. This is not a portable HPL call.

| Principle | Prey implementation | SOMA assessment |
|---|---|---|
| One authoritative goal | `src/dll/AnimIkTakeover.cpp:343..418`: convert controller goal, clamp it, then write the same absolute target to native IK | **Concrete gap below:** SOMA's arm and wrist writers disagree about position |
| Actual transform chain | Invert captured animation model-to-world quaternion, translation and scale | Already substantially aligned: `HPLHandsMath::BuildPostTransformForWorldTarget` uses full parent/local affine inverses; do not replace it with yaw-only inversion |
| Coherent tracking and owner | Tracking-frame snapshot plus clean native camera anchor; selected attachment/character identity and binding generation | SOMA has shared hand snapshot, exact hands entity/hierarchy checks and player-frame guards. Preserve these; avoid separately re-sampling camera calibration per consumer |
| Correct consumer timing | Native ADIK before weapon bone attachments; late skin-only changes were insufficient | HPL's node -> deformation palette -> CPU skin/VBO chain is already mapped and sampled. Continue proving that chain, not just wrist node positions |
| Ownership before locking | Non-owner animation callbacks bypass before taking the IK lock | Prey's native jobs differ from HPL; copy the rule only where an HPL multi-owner path exists, not its lock/ABI blindly |
| Lifecycle-safe calibration | Per-hand pending request, commit only after successful target write, reference/binding invalidation and recovery | Good test pattern. SOMA geometric palm calibration is model-relative and need not be invalidated by every reference-space change; distinguish it from first-pose fallback calibration |

Read the original Prey integration contracts:
`docs/RE-H021-INTEGRATION-AUDIT-2026-09-07.md`,
`docs/RE-H021-ABSOLUTE-WRIST-ARM-CHAIN-WEAPON-AIM-2026-09-07.md`,
`src/common/AnimIk.cpp`, `include/preyvr/AnimIk.h`, and
`src/dll/AnimIkTakeover.cpp` under `D:/Dev Debug/PreyVR`.

No Prey implementation code was copied, and no claim is made that CryEngine's
PDB/native solver offsets apply to SOMA.

## A Concrete SOMA Goal Conflict

Current working source, active build family `0.95.9-player-space-recovery`:

1. `HPLHandsBridge.cpp:1667`, `ApplyPlayerHandsArmIK`, sets `target` from raw
   `tracking.targets[hand].position` at approximately line 1760. Clavicle reach,
   elbow pole and `SolveTwoBone` use this point. It aims the Arm_1/Arm_6 chain
   at `solution.wrist`, which may be reach-clamped.
2. `ApplyPlayerHandsWristPositions`, lines 2019..2071, then builds a different
   position: raw controller target **plus** view-yaw calibration offsets. It
   does not consume `solution.wrist` or the previous reach-clamp decision.
3. Both operations run consecutively in the active and retained paths at
   lines 3329/3330 and 3946/3947. This is not an unused helper discrepancy.

Release offsets (`config/somavr.release.ini:305`) are outward 0.03 m, vertical
-0.04 m, forward -0.04 m. With a valid orthonormal yaw basis:

```text
|wrist requested point - IK requested point|
    = sqrt(0.03^2 + 0.04^2 + 0.04^2)
    = 0.064031 m
```

Thus the solver and final wrist writer are approximately **6.4 cm apart**, even
before reach clamping. A final unconstrained wrist translation can additionally
undo the solver's reach policy and stretch the distal mesh. The source proves
the competing targets; it does **not** prove that this explains every observed
wrist twist, shoulder reversal, or inter-eye lag.

Highest-value implementation candidate: compute one calibrated desired wrist
goal per hand from one tracking/camera snapshot, feed that to the arm solver,
then explicitly share its selected reachable endpoint with the wrist writer.
Keep the IK-off fallback. Decide and report how unreachable tracking is handled;
do not silently copy Prey's controller-travel gain and call it 1:1.

Offline acceptance: translated/scaled parents, body yaw 0/90/180 degrees,
mirrored left/right offsets, reachable/unreachable targets, IK-off fallback,
and no second mutation for the replay eye. A bounded row should record desired
goal, solved endpoint, actual final wrist, clamp distance, owner, pose/player
frame and eye. That will distinguish a solver issue from a later writer.

## Why A Correct Wrist Can Still Look Twisted

SOMA's full mapped hierarchy is not just shoulder -> elbow -> wrist:

```text
j_Root -> Clavicle -> Shoulder -> Arm_1..5 -> Elbow_1/2
       -> Arm_6..10 -> Wrist -> finger chains
```

The current two-hinge prototype drives Arm_1 and Arm_6 plus the wrist. The
numbered arm nodes also distribute skin deformation; they are not eleven
independent anatomical hinges. Abrupt wrist orientation without distributed
forearm twist can still deform the skin incorrectly. This is an existing
documented gap, not a newly discovered rig: see
`AUTHORED_STATES_AND_VISIBLE_HANDS_RE.md:548` and `FUTURE_SYSTEMS_RE.md:52`.

Prey's native solver owns its own propagation, which is the useful architectural
comparison. Before implementing an HPL swing/twist distribution, establish the
authored axes and skin weights on both sides, and test mirrored palm parity.
The shipped `hands_human.dae` has centimeter units and a Y-up asset declaration;
those authoring units do not by themselves establish the runtime root scale.
Do not add another blanket 90-degree rotation to hide the deformation problem.

## Next Discriminating Test

After a shared-goal build, keep the head still and test one hand at a time:
close reach, comfortable reach, then maximum reach; roll the wrist slowly at
each distance. Repeat after a 90-degree snap turn and a physical body turn.
Check the inactive hand for unintended motion. Record desired/solved/final
wrist residuals and the existing node/palette pair witness.

Use Ctrl+F10 for paired eye RGB during stationary reach and during locomotion.
If node/palette hashes match but visible arms differ, inspect the CPU skin/VBO
consumption path and camera/pair metadata before tuning elbow damping. Existing
xr-tape poses are a submission control, not proof of the arm pixels.

Priority: **shared goal contract first**, then measured forearm twist
distribution, then calibration/owner lifecycle hardening where receipts show
a gap. No broad solver replacement or reach-gain tuning is justified yet.

Related depth result: `HPL_SCENE_DEPTH_PROOF_2026-09-09.md`.
