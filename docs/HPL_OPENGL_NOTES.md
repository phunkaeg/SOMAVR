# HPL OpenGL Notes

Source references are from HPL2, not SOMA's HPL3, so treat them as structural hints rather than proof.

The local SOMA install is at `G:\SteamLibrary\steamapps\common\SOMA\`. Its `config\game.cfg` keeps the same camera shape expected from HPL2: player `FOV="70"`, `FarClipPlane="1000"`, and `NearClipPlane="0.03"`.

## Frame Boundary

`HPL2\core\sources\engine\Engine.cpp` calls:

```cpp
mpGraphics->GetLowLevel()->SwapBuffers();
mpUpdater->RunMessage(eUpdateableMessage_OnPostBufferSwap);
```

`cLowLevelGraphicsSDL::SwapBuffers` calls `SDL_GL_SwapBuffers`. On Windows, that should eventually hit `gdi32!SwapBuffers`, which makes `SwapBuffers` the most stable first frame-boundary hook.

## Projection Path

`cCamera` owns FOV/aspect/near/far and lazily builds projection/frustum data:

- default FOV: 70 degrees
- default aspect: 4:3
- default near/far in HPL2: `0.05 / 1000`
- projection built by `cMath::MatrixPerspectiveProjection`

`iRenderFunctions::SetFrustumProjection` passes `cFrustum::GetProjectionMatrix()` into `iLowLevelGraphics::SetMatrix(eMatrix_Projection, ...)`.

`cLowLevelGraphicsSDL::SetMatrix` does:

```cpp
SetMatrixMode(aMtxType);
glLoadIdentity();
cMatrixf mtxTranpose = a_mtxA.GetTranspose();
glLoadMatrixf(mtxTranpose.v);
```

This makes `glMatrixMode(GL_PROJECTION)` followed by `glLoadMatrixf` a likely projection probe for fixed-function or compatibility rendering.

## GLSL Matrix Path

`cGLSLProgram::SetMatrixf` does:

```cpp
if(mlCurrentProgram != mlHandle) Bind();
glUniformMatrix4fv(mvParameters[alVarId].mlId, 1, true, aMtx.v);
```

Useful hook set:

- `glGetUniformLocation` to map `(program, location) -> uniform name`.
- `glUseProgram` to track current shader program.
- `glUniformMatrix4fv` to identify projection/view/world matrices and their upload cadence.

## RenderDoc Clue

The reported RenderDoc unsupported call `glTexEnvfv` is present in HPL2:

```cpp
glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, &vColor[0]);
```

That points at an OpenGL compatibility profile path. The initial probe should avoid assuming a modern core-only renderer.

## apitrace-Measured Projection (2026-08-10, confirmed)

A vanilla trace of `Soma_NoSteam.exe` under apitrace 14.0 (GL) decoded the main-scene projection
uniform `glUniformMatrix4fv(program=884, location=1)` at confidence 1.0:

- FOV_y = 70.0°, FOV_x = 102.4°, aspect 16:9, near = 0.03, far = 998.67
- OpenGL right-handed, normal (`-1..1`) depth, not reversed-Z

This confirms the expected `game.cfg` camera shape (`FOV=70`, `NearClipPlane=0.03`, `FarClipPlane~1000`)
at the driver layer, and that HPL3 uploads projection as a GLSL uniform (not fixed-function
`glLoadMatrixf` — negative for the main scene). The moving view-projection uploads via
`glUniformMatrix4fv(program=822, location=1)` (`camera_moves=true`). Full detail: `future-hook-map.md`.
The numeric GL program names belong to this trace and may change between runs;
matrix semantics and the native frustum packet are the stable anchors.

## apitrace Native-Stereo Resource Evidence (2026-08-23)

The same vanilla trace supplies two constraints that are invisible at the
camera-uniform surface:

- It contains `14,828` `glBeginQuery` calls, the same number of `glEndQuery`
  calls, and `29,609` query-result reads. In frame 576 HPL uses query IDs
  `1..4`; frame 577 first polls availability/result for those IDs, then begins
  new queries with the same IDs in reverse order. This is an immediate reusable
  query pool, not an eye-local history. A second world render in the same game
  frame must prove query isolation or accept that replay-eye work may replace
  first-eye visibility state.

- It contains `6,684` `glCopyTexSubImage2D` calls. A gameplay example at call
  `964252` copies a partial `942x888` rectangle from `(978,0)` into texture 35,
  immediately before translucent program 949 draws. Released HPL2 source
  independently shows this exact shape: each refractive object's screen clip
  rectangle is copied into a shared refraction texture immediately before the
  object draw. Full-screen copies also exist for post/edge-smoothing work, so
  copy dimensions and render-stage ownership must be retained in diagnostics.

Trace-local program 949 links fragment shader 368. Its generated source names
`aRefractionMap` at binding 2, `aSceneDepth` at binding 7, and the
`cTranslucentTypeArguments` UBO containing view-projection, projection, view,
and all three inverses. Program 948's adjacent fragment shader 578 is a
non-refractive translucent variant but uses the same camera UBO. This confirms
that `glUniformMatrix4fv` interception alone cannot cover translucent HPL3
camera state; the native frustum/renderer packet or the UBO upload must own
stereo.

`0.92.0-native-stereo-evidence` adds bounded observation for both risks while
the existing continuous replay/control lane is active.
`hpl_occlusion_query_summary` reports query IDs reused across first and
replay eyes; `hpl_framebuffer_copy_summary` reports shared destination textures
written by both eyes. The hooks activate only with the existing dual-render
continuous/replay controls and ignore SOMAVR's private GL work.
