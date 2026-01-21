# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Never look at other branches**

**Never try to compile the code**

## Overview

This is a Windows port of Quake III Arena that supports multiple rendering backends (OpenGL, Direct3D 11) on x86/x64 architectures. The port targets Windows Desktop (Win32) and Windows 8/RT with modern APIs: Direct3D 11.1/11.2, XAudio2, and XInput.

## Building the Project

### Desktop Edition (Win32)

1. Open the solution: `code\Quake3Win32_VS2019.sln` in Visual Studio 2019 or later
2. Set `Quake3Win32_VS2013` as the startup project
3. Choose platform (x64 recommended) and configuration:
   - `Debug` or `Release` - Vanilla Quake 3 Arena
   - `Debug TA` or `Release TA` - Team Arena expansion
4. Set Working Directory in Project Properties > Debugging to `$(SolutionDir)..`
5. Build the solution (builds all dependencies: renderer, botlib, quake3, game DLLs, UI DLLs)
6. Run with command-line argument `+set sv_pure 0` (required when using VM DLLs)

### Required Game Assets

- Place the `baseq3` directory (containing pak0.pk3, pak1.pk3, etc.) in the repository root
- For Team Arena, place the `missionpack` directory in the root

### Windows 8 Edition

- Solution: `code\win8\Quake3Win8_VS201X.sln`
- WinRT package for Windows Store deployment
- Only supports vanilla Q3A (not Team Arena)
- Always uses Direct3D 11, XAudio2, XInput

## Codebase Architecture

### Renderer Abstraction Layer (`tr_layer.h`)

The core architectural pattern is a **function pointer-based abstraction layer** that allows swapping rendering backends at runtime. This enables:
- Side-by-side comparison of renderers (`r_driver proxy`)
- Runtime switching between OpenGL, D3D11, and Vulkan

Key files:
- `code/renderer/tr_layer.h` - Declares ~40 `GFX_*` function pointers
- `code/renderer/tr_layer.c` - Defines the function pointers (initialized by driver)
- `code/renderer/gl_driver.c` - OpenGL implementation
- `code/d3d11/d3d_driver.cpp` - Direct3D 11 implementation

### Renderer Backend Implementations

Each graphics backend must implement the `GFX_*` interface defined in `tr_layer.h`:

**Core Functions:**
- Initialization/Shutdown: `GFX_Shutdown`, `GFX_UnbindResources`
- Image Management: `GFX_CreateImage`, `GFX_DeleteImage`, `GFX_UpdateCinematic`
- State Management: `GFX_SetState`, `GFX_ResetState2D`, `GFX_ResetState3D`
- Drawing: `GFX_DrawStageGeneric`, `GFX_DrawSkyBox`, `GFX_DrawImage`
- Matrices: `GFX_SetProjectionMatrix`, `GFX_SetModelViewMatrix`

**Example Backend Structure (D3D11):**
```
code/d3d11/
  d3d_driver.cpp/h    - Entry points (D3DDrv_* functions)
  d3d_common.cpp/h    - Device initialization
  d3d_state.cpp/h     - Pipeline state management
  d3d_draw.cpp/h      - Drawing commands
  d3d_image.cpp/h     - Texture management
  d3d_shaders.cpp/h   - Shader loading/compilation
```

### Module Structure

The engine is split into several modules built as separate projects:

- **quake3** (`code/qcommon/`) - Core engine, networking, console, cvars
- **renderer** (`code/renderer/`) - Platform-agnostic renderer frontend
- **botlib** (`code/botlib/`) - Bot AI pathfinding/navigation
- **Splines** (`code/splines/`) - Curve/spline math utilities
- **Win32 main** (`code/win32/`) - Platform layer (windowing, input, main loop)

Game logic modules (built as DLLs):
- **game** (`code/game/`) - Server-side game logic
- **cgame** (`code/cgame/`) - Client-side game rendering/prediction
- **ui** (`code/ui/`) - Team Arena UI
- **q3_ui** (`code/q3_ui/`) - Vanilla Q3A UI

### Shader Pipeline

Shaders are authored in HLSL for D3D11:

- HLSL source: `code/hlsl/*.hlsl`

Standard shaders:
- `genericst` - Single-texture rendering (most surfaces)
- `genericmt` - Multi-texture (lightmapped surfaces)
- `skybox` - Sky rendering
- `fsq` - Full-screen quad (2D UI, cinematics)

### State Management (`tr_state.h`)

The renderer uses bitflags (`GLS_*`) to manage render state:
- `GLS_SRCBLEND_*` / `GLS_DSTBLEND_*` - Blend modes
- `GLS_DEPTHTEST_DISABLE` - Depth testing
- `GLS_DEPTHMASK_TRUE` - Depth writing
- `GLS_ATEST_*` - Alpha testing modes

Backends translate these flags into their native state objects (D3D11 blend states etc.)

### Important Caveats

**Depth Range Difference:**
- OpenGL NDC Z: [-1, 1]
- D3D11 NDC Z: [0, 1]

When porting shaders, convert OpenGL depth to Vulkan/D3D:
```glsl
ndcZ = (ndcZ + 1.0) * 0.5;
```

**Code Markers:**
- Search for `@pjb` to find modifications made by the original porter
  **Do not add `@pjb` to new comments**
- Code follows original Q3A structure where possible to minimize invasiveness

### Runtime Configuration

Console variables (cvars) for switching backends:
- `r_driver opengl` - Use OpenGL renderer
- `r_driver d3d11` - Use Direct3D 11 renderer
- `r_driver proxy` - Side-by-side comparison mode
- `snd_driver xaudio` / `snd_driver dsound` - Audio backend
- `in_gamepad 1/0` - Enable/disable XInput gamepad

Widescreen setup:
```
r_mode -1
r_customwidth 1920
r_customheight 1080
```

## Development Workflow

### Adding a New Rendering Backend

1. Create a new directory under `code/` (e.g., `code/vulkan/`)
2. Implement all `GFX_*` functions declared in `tr_layer.h`
3. Study `code/d3d11/` as a reference implementation
4. Handle state management by mapping `GLS_*` flags to native API constructs
5. Implement shader loading/compilation for your target shading language
6. Add driver initialization in `code/renderer/tr_init.c`


### Known Issues

- `r_smp 1` causes deadlocks (original Q3A bug)
- `r_shadows 2` (stencil shadows) not implemented in D3D11
- Gamepad menu navigation is difficult
- Cinematics and HUD stretch in widescreen
- Win8 broadcast sockets not implemented (use `connect <ip>` from console)

### Debugging

- RenderDoc works for graphics debugging
- Set breakpoints in `GFX_*` functions to trace rendering calls
- Use `r_driver proxy` to visually compare OpenGL vs. D3D11 output side-by-side
- Check `games.log` in `baseq3/` for engine logs

## File Organization

```
code/
  renderer/       - Platform-agnostic renderer (tr_*.c)
  d3d11/          - Direct3D 11 backend
  client/         - Client-side networking, sound, input
  server/         - Server-side networking, game hosting
  qcommon/        - Common code (cvars, files, console)
  game/           - Server-side game logic (VM)
  cgame/          - Client-side game logic (VM)
  ui/             - Team Arena UI (VM)
  q3_ui/          - Vanilla Q3A UI (VM)
  botlib/         - Bot AI
  win32/          - Windows platform layer
  win8/           - WinRT platform layer
  hlsl/           - HLSL shaders for D3D11
  spirv/          - Compiled SPIRV shaders (some builds)

baseq3/           - Game assets (not in repo, user-provided)
missionpack/      - Team Arena assets (not in repo, user-provided)
tools/            - Map compiler, assembler, LCC compiler
```
