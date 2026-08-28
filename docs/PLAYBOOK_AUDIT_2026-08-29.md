# VR Modding Playbook Delta Audit - 2026-08-29

Status: applicable static/code items incorporated in
`0.95.2-playbook-conformance`; live render/performance gates remain open by
design.

## Scope And Method

This is a delta from `PLAYBOOK_AUDIT_2026-08-27.md`, not a replacement for it.
It compared SOMAVR with the playbook changes dated 2026-08-28 and 2026-08-29,
including the executable A1-A5 reference annexes, chapters 02, 06, 07, 09, 11,
13, 14, 17, 18, and 19, `pattern-catalog.md`, `failure-atlas.md`, and the fleet
bottleneck map.

Both knowledge graphs were used as orientation indexes:

- local `graphify-out/graph.json` for SOMAVR ownership and exact document/code
  leads;
- the reconciled cross-engine documentation graph for same-concept leads from
  SS2VR and BioshockVR.

Every cross-project hit was rechecked against SOMAVR source before being graded.
The playbook's built reference executable also passed all 27 tests and 18,335
checks. Its README currently names `tools/verify.py`, but that file is absent;
SOMAVR does not depend on the missing helper.

## Incorporated In 0.95.2

### A1 / TEST-006 / TEST-007 - failure mechanism and margin

The 0.95.1 runtime guard was already correct: normalize positive authored
scale, require mutually orthogonal axes, and require determinant near `+1`.
The missing piece was executable proof at the mechanism boundary.

0.95.2 now:

- classifies non-finite, degenerate-column, non-orthogonal,
  improper-handedness, and quaternion-conversion failures;
- proves a reflected orthonormal basis passes a weak `abs(det)==1` check but is
  rejected by the production guard;
- accepts a fixed near-tolerance shear and rejects a fixed outside-tolerance
  shear, pinning useful margin rather than only testing an obviously bad case;
- exercises identity through near-180-degree rotations on principal and mixed
  axes, comparing matrix-vector and independent quaternion-vector paths.

The validation tolerance and all existing fail-closed ownership remain
unchanged.

### A3 - asymmetric projection proof

SOMAVR already constructs the GL right-handed OpenXR projection from tangent
extents and preserves asymmetric runtime FOV. Tests now additionally prove that
left, right, down, and up near-plane points map to NDC `-1/+1`, that the tested
frustum is genuinely off-axis, and that averaging angles would produce a
different result.

### Diagnostic attribution

`hpl_arm_body_summary` now distinguishes malformed controller forward/up input
from each native wrist-matrix rejection class. This turns a future hand fallback
from a generic counter into an actionable next RE question without logging per
draw/per-bone noise.

## Confirmed Already Aligned

- **A2 pose handoff:** runtime/input snapshots are mutex-protected coherent
  packets. A seqlock is an optimization only if live contention evidence
  justifies it.
- **A2 recenter epoch:** the stored generation begins at zero internally but is
  incremented before tracking becomes visible, then increments once per
  recenter. Consumers rebase on generation change; no consumer accepts a
  default epoch as a calibrated baseline.
- **AFR pair ownership:** one wait, one locate, and one upcoming-render
  prediction feed a complete immutable pair. Eye two replays eye one's native
  camera packet and OpenXR snapshot with a 100 ms expiry.
- **Adaptive/temporal state:** SSAO, temporal projection/history, tone mapping,
  exposure, grading transitions, exposure windows, and film grain use
  capture/replay/commit semantics once per pose pair. Intra-frame bloom scratch
  remains intentionally shared.
- **A4 hook safety:** unique signatures, expected-byte verification, suspended
  peer-thread/IP guards, fail-closed native memory access, hit counters,
  re-entrancy/own-GL scope, module-relative evidence, and transactional restore
  are present and tested.
- **A5 bounded capture:** matrix/render/terminal/skeleton diagnostics are armed,
  frame/sample bounded, self-terminating, and separate log limits from live
  behavior.
- **OpenXR safety:** every begun frame submits projection content, layer arrays
  have headroom, runtime limits are queried/enforced, important layers are
  prioritized, and F10 stop destroys the session/instance symmetrically.
- **Packaging:** runtime roots are module-relative/overridable, package config is
  source-controlled, injector wait results are checked, installs are
  manifest-owned, and uninstall removes only explicit owned files.
- **Data over code:** exact entity calibration lives in validated profile data,
  while address registry entries remain evidence for one build and signatures
  remain source authority.

## Evidence-Gated, Not Missing

- `BN-RND-001`: per-eye refraction, scene-color scratch, occlusion-query reuse,
  and remaining post effects still require the next matched headset log. The
  read-only owners and counters are already in the package.
- `BN-PERF-001`: OpenGL capture/submission GPU timestamps, CPU phase timing,
  pair freshness, replay draw cost, and Wait/Begin/End attribution need a live
  scene before choosing native stereo, D3D11 interop, or a wait-ahead worker.
- `BN-UI-001`: broad authored-state/effect coverage remains gameplay testing,
  not a static-code completion claim.
- `INPUT-004/005`: locomotion currently reaches HPL's semantic analog method
  after SOMAVR radial conditioning. There is no evidence of a second square
  deadzone at that seam, so inverse conditioning would be speculative and could
  regress accepted movement.
- Engine-native IK discovery remains one bounded static/live RE question; the
  current 34-node bilateral solver and palm-bone cluster are real and working.
- A substitute OpenXR runtime and public extension surface remain future test
  infrastructure/release-ecosystem work, not current T3 blockers.

## Not Applicable To This Target

Device-vtable rehooking for recreated D3D COM objects, D3D adapter/LUID policy,
x87 control, 32-bit injection, D3D12 queue ownership, managed-plugin guidance,
source-port bit-exact original-render comparison, and multiplayer policy do not
map to the supported 64-bit HPL3/OpenGL injector.

Playbook `STR-010` is interpreted through SOMAVR's OpenXR contract: an invalid
fresh pair is not presented as fresh, but a begun XR frame still submits a
bounded coherent hold and then black projection. It must never become a
zero-layer submit.

## Remaining Autonomous Order

No further runtime architecture change is justified without live evidence.
The next autonomous work that remains safe is maintenance only: keep Graphify
and registries current, run package/release gates, and analyze submitted logs.
The next material decisions require the matched headset route in
`NEXT_LIVE_EVIDENCE.md`.
