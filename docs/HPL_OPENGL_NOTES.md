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
