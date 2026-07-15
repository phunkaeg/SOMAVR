# Native Contact Haptics Reverse Engineering

Date: 2026-07-15

## Confirmed Native Path

SOMA's Newton physics update at `0x1405548b0` walks `0x60`-byte contact records
after simulation. Ghidra and released HPL2 source agree on this dispatch:

```text
cPhysicsWorldNewton::Update
  -> cSurfaceData::OnImpact(normal speed, position, contacts, body)
  -> cSurfaceData::OnSlide(tangent speed, position, contacts, body, other body)
  -> iPhysicsBody::OnCollide for each body
```

The confirmed HPL3 functions are:

| Address | Function | Stable evidence |
| --- | --- | --- |
| `0x1405548b0` | `HPL3_cPhysicsWorldNewton_Update` | Newton step, contact-record loop, material priority, paired body callbacks |
| `0x14032f0e0` | `HPL3_cSurfaceData_OnImpact` | Normal speed, contact position/count, body; thresholded native Impact effect |
| `0x14032f380` | `HPL3_cSurfaceData_OnSlide` | Tangent speed, contact position/count, both bodies; sustained Scrape lifecycle |

## 0.58 Policy

`HPLContactHapticsBridge` hooks only `OnImpact` and always executes SOMA first.
The OpenXR pulse requires all of the following:

- controller haptics and contact haptics enabled;
- exact player state `1` (Grab) with no authored-camera ownership;
- a fresh, position-tracked dominant grip resolved through the established HPL
  world-pose bridge;
- finite native speed and contact position;
- contact within the configured dominant-grip radius;
- speed at or above the configured minimum;
- no accepted pulse inside the duplicate-callback cooldown.

Amplitude is a bounded linear map from native normal speed. Output targets only
the dominant hand, unlike SOMA's bilateral script-authored rumble. The original
surface sound, particles, contact callback, physics state, and gamepad behavior
are never changed.

## Known Boundary

HPL selects surface callbacks by material priority. The `body` argument at
`OnImpact` can therefore be the higher-priority material side rather than the
grabbed body. `0.58` deliberately does not infer ownership from that pointer;
Grab state plus spatial proximity is the conservative filter. Logs retain body,
speed, contact count, distance, hand, amplitude, and acceptance counts so live
testing can measure false positives.

## Next Evidence

Test light taps, hard impacts, resting contact, rapid repeated contact, impacts
near but unrelated to the held object, two-material objects, release, tracking
loss, and authored camera transitions. If proximity produces false positives,
the next RE target is the exact `mpCurrentBody` field in SOMA's Grab state. Only
after impact feedback is accepted should `OnSlide` receive a read-only tangent-
speed/body probe for sustained friction haptics.
