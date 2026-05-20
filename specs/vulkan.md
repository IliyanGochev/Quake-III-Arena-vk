# Vulkan Graphics Driver Implementation Spec
### Quake 3 Engine Fork

---

## 1. Overview

This document specifies the implementation of a Vulkan-based graphics backend for the Quake 3 engine fork. The engine already exposes an abstracted driver interface with working OpenGL and D3D11 implementations. The Vulkan driver must conform to that same interface contract while exploiting Vulkan's explicit control over synchronization, memory, and render passes.

The target is a fully functional, frame-correct Vulkan driver that can render the complete Quake 3 feature set — world geometry, BSP surfaces, shader-driven surfaces, dynamic lights, fog, skyboxes, UI/2D, and screenshots — at parity with the existing drivers.

---

## 2. Scope & Constraints

**In scope**
- Full implementation of every entry point in the engine's graphics driver interface
- Vulkan 1.1 as the minimum API version (1.2/1.3 features used where available via feature detection)
- Windows (Win32 surface), Linux (XCB/Xlib surface), and macOS (Metal via MoltenVK) platform targets
- Shader compilation: GLSL → SPIR-V via `glslangValidator` / `shaderc` at build time; no runtime GLSL compilation
- Runtime pipeline cache (serialized to disk, keyed by pipeline state hash)
- Validation layer integration in debug builds

**Out of scope**
- Ray tracing extensions
- Video/cinematic decoding (handled outside the graphics driver)
- Networking or game logic changes
- Modifications to the existing OpenGL or D3D11 drivers

---

## 3. Driver Interface Contract

> **Agent instruction:** Before writing any implementation code, read the existing interface header (likely `renderer/tr_driver.h` or equivalent) and both reference implementations (`tr_gl.c` / `tr_d3d11.cpp`). Treat those as the ground truth for all entry point signatures and calling conventions. The sections below describe *what* must be implemented; the interface header defines *how* it is called.

### 3.1 Entry Points (Expected)

| Category | Entry Points |
|---|---|
| Lifecycle | `Init`, `Shutdown`, `BeginFrame`, `EndFrame` |
| Scene | `BeginScene`, `EndScene` |
| World geometry | `DrawBSPSurfaces`, `DrawPolygons` |
| Meshes | `DrawMesh`, `DrawSkinnedMesh` |
| Shaders/materials | `SetShader`, `SetTexture`, `SetLightmap` |
| State | `SetViewport`, `SetScissor`, `SetModelViewProjection` |
| 2D / UI | `Draw2DQuad`, `DrawStretchPic` |
| Screenshots | `ReadPixels` |
| Misc | `MarkShadersForReload`, `GetInfo` |

> If the actual interface differs, implement exactly what the header declares. Do not add, remove, or rename entry points.

---

## 4. Architecture

### 4.1 Module Structure

```
renderer/vulkan/
  vk_init.c/h          -- Instance, device, surface, swapchain creation
  vk_swapchain.c/h     -- Swapchain management and resize handling
  vk_memory.c/h        -- VMA integration, buffer/image allocators
  vk_pipelines.c/h     -- Pipeline cache, PSO creation, pipeline variants
  vk_renderpass.c/h    -- Render pass and framebuffer management
  vk_descriptors.c/h   -- Descriptor pool, set layouts, set allocation
  vk_upload.c/h        -- Staging buffer pool, texture/buffer upload
  vk_cmd.c/h           -- Command pool, per-frame command buffer management
  vk_image.c/h         -- Image creation, views, samplers
  vk_shader.c/h        -- SPIR-V loading, shader module creation
  vk_draw.c/h          -- High-level draw call recording
  vk_2d.c/h            -- 2D / UI rendering path
  vk_debug.c/h         -- Validation layer callback, debug markers
  vk_main.c/h          -- Driver interface implementation (entry points)
```

### 4.2 Frame Lifecycle

```
BeginFrame()
  └─ Acquire swapchain image (vkAcquireNextImageKHR)
  └─ Wait on in-flight fence for this frame slot
  └─ Reset per-frame command pool
  └─ Begin primary command buffer

  [Engine calls Draw* and Set* functions]
  └─ Commands are recorded into primary command buffer

EndFrame()
  └─ End primary command buffer
  └─ Submit to graphics queue (with acquire semaphore wait, render semaphore signal)
  └─ Present swapchain image (with render semaphore wait)
  └─ Advance frame index (ring of FRAME_OVERLAP frames, default 2)
```

### 4.3 Frame Overlap

Use a ring buffer of `FRAME_OVERLAP = 2` frame slots. Each slot owns:
- One primary command pool + command buffer
- One in-flight fence
- Per-frame dynamic uniform buffer region (from a mapped persistent buffer)
- Per-frame descriptor sets for dynamic data

---

## 5. Initialization Sequence

### 5.1 Instance Creation
1. Enumerate available instance extensions; require `VK_KHR_surface` + platform surface extension.
2. Enable `VK_EXT_debug_utils` in debug builds.
3. Create `VkInstance` with app info (`apiVersion = VK_API_VERSION_1_1`).
4. Register debug messenger callback (debug builds only).

### 5.2 Physical Device Selection
1. Enumerate physical devices; score each on:
   - Discrete GPU preferred over integrated
   - Required extension support (see §5.3)
   - Required feature support (see §5.4)
   - Swapchain adequacy (at least one supported format and present mode)
2. Select highest-scored device. Log device name, driver version, and Vulkan API version.

### 5.3 Required Device Extensions
- `VK_KHR_swapchain`
- `VK_KHR_maintenance1` (negative viewport height for flipping Y)

Optional (enable if available, no hard dependency):
- `VK_EXT_memory_budget`
- `VK_KHR_dedicated_allocation`
- `VK_EXT_debug_marker` (debug builds)

### 5.4 Required Features
- `samplerAnisotropy`
- `fillModeNonSolid` (wireframe debug mode)
- `wideLines` (enable if available; degrade gracefully if not)

### 5.5 Queue Family Selection
- Graphics queue family: must support `VK_QUEUE_GRAPHICS_BIT`; prefer one that also supports present
- Transfer queue family: dedicated transfer queue if available; fall back to graphics queue
- Minimize distinct queue families to simplify ownership transfers

### 5.6 Logical Device & VMA
1. Create logical device with selected queue families.
2. Initialize **Vulkan Memory Allocator (VMA)**. Pass `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` if extension available.

### 5.7 Swapchain Creation
- Preferred format: `VK_FORMAT_B8G8R8A8_SRGB` with `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`; fall back to first available
- Preferred present mode: `VK_PRESENT_MODE_MAILBOX_KHR` if vsync disabled, `VK_PRESENT_MODE_FIFO_KHR` otherwise
- Image count: `min(surfaceCaps.minImageCount + 1, surfaceCaps.maxImageCount)`
- Transform: use `currentTransform` from surface capabilities

### 5.8 Render Pass Setup

Create a single main render pass with:
- Color attachment: swapchain format, `loadOp = CLEAR`, `storeOp = STORE`, final layout `PRESENT_SRC_KHR`
- Depth/stencil attachment: `VK_FORMAT_D24_UNORM_S8_UINT` (or `D32_SFLOAT_S8_UINT` as fallback), `loadOp = CLEAR`, `storeOp = DONT_CARE`
- One subpass; standard subpass dependency for external → subpass and subpass → external transitions

Create one framebuffer per swapchain image.

---

## 6. Pipeline Management

### 6.1 Shader Compilation

All shaders are compiled offline to SPIR-V and embedded as C byte arrays or loaded from disk at startup (match the approach used by the other drivers for asset loading). No runtime GLSL compilation.

Required shader pairs (at minimum):

| Name | Purpose |
|---|---|
| `world_opaque` | World surfaces, lightmapped |
| `world_alpha` | Alpha-tested world surfaces |
| `generic_unlit` | Entities, dynamic geometry |
| `skybox` | Skybox rendering |
| `fog` | Fog volume rendering |
| `2d` | UI, HUD, console |
| `lightmap_only` | Debug lightmap visualization |

### 6.2 Pipeline State Objects

Enumerate pipeline variants as a bitmask or struct:
- Blend mode (opaque / alpha-test / alpha-blend / additive)
- Depth write on/off
- Depth compare op (LESS / LESS_OR_EQUAL / ALWAYS)
- Cull mode (back / front / none)
- Fill mode (solid / wireframe)
- Stencil on/off

Create PSOs lazily on first use. Store in a hash map keyed on the full state + shader combination.

### 6.3 Pipeline Cache

- Create a `VkPipelineCache` object at startup; load serialized cache data from `<basepath>/vk_pipeline_cache.bin` if it exists.
- On shutdown, serialize and write the cache back to disk.
- Invalidate cache file if device/driver version changes (store metadata in a header).

### 6.4 Push Constants & Descriptor Set Layout

Use a simple, fixed descriptor set layout:

- **Set 0** — Per-frame globals: view/projection matrices, time, screen size (uniform buffer)
- **Set 1** — Per-draw: model matrix, surface parameters (push constants preferred if ≤ 128 bytes; UBO otherwise)
- **Set 2** — Textures: diffuse sampler, lightmap sampler, optional normal/specular (combined image samplers)

---

## 7. Memory Management

### 7.1 Buffer Types

| Buffer | Allocation strategy |
|---|---|
| Vertex / index (static world) | `VMA_MEMORY_USAGE_GPU_ONLY`, uploaded once via staging |
| Vertex / index (dynamic) | Per-frame ring buffer, `HOST_VISIBLE | HOST_COHERENT` |
| Uniform (per-frame globals) | Persistent mapped, one region per frame slot |
| Staging | Pool of reusable `HOST_VISIBLE` buffers; return to pool after copy fence signals |

### 7.2 Texture Upload

1. Allocate staging buffer sized to mip chain.
2. Copy pixel data into staging buffer (mapped pointer).
3. Record `vkCmdCopyBufferToImage` on the transfer command buffer with appropriate image layout transitions (`UNDEFINED → TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`).
4. Submit transfer; signal a fence. Free staging buffer after fence signals (defer to next `BeginFrame` of the same slot).

### 7.3 Dynamic Geometry

Use a per-frame vertex ring buffer sized generously (e.g., 4 MB) to handle dynamic surfaces, decals, and UI quads. Track the current offset; reset to 0 at `BeginFrame`. If overflow occurs, flush and wrap (log a warning).

---

## 8. Texture & Sampler Management

- Maintain a texture cache keyed on image name/path (mirrors what OpenGL driver does with texture objects).
- Texture formats: upload in `VK_FORMAT_R8G8B8A8_UNORM`; lightmaps in `VK_FORMAT_R8G8B8A8_SRGB` (or unorm if engine does gamma elsewhere).
- Generate mipmaps via `vkCmdBlitImage` on the GPU after initial upload, or accept pre-generated mips from the engine.
- Sampler variants: anisotropic (world textures), bilinear (UI), nearest (console chars). Create once, reuse.

---

## 9. Synchronization

- **Acquire semaphore** per frame slot: signals when swapchain image is available
- **Render semaphore** per frame slot: signals when rendering is complete (waited on by present)
- **In-flight fence** per frame slot: CPU waits here before reusing a slot's resources
- **Transfer fence**: signals when transfer commands complete; CPU-side wait before freeing staging buffers
- No manual `vkQueueWaitIdle` or `vkDeviceWaitIdle` except during `Shutdown` and full swapchain recreation

---

## 10. Swapchain Resize / Recreation

On `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` returned from acquire or present:
1. Call `vkDeviceWaitIdle`.
2. Destroy framebuffers, swapchain image views, depth image.
3. Recreate swapchain (reuse old swapchain handle for efficiency where supported).
4. Recreate framebuffers.
5. Do **not** recreate pipelines, descriptor pools, or command pools.

The engine should also call the driver's resize entry point when the window is resized; handle this the same way.

---

## 11. Quake 3–Specific Rendering Concerns

### 11.1 Coordinate System

Quake 3 uses a right-handed coordinate system with Z-up. Vulkan uses Y-down in NDC. Handle this by:
- Negating the Y component in the projection matrix, **or**
- Using `VK_KHR_maintenance1` to set a negative viewport height (flips Y in clip space)

Match the convention used by the D3D11 driver for consistency.

### 11.2 BSP Surface Batching

The engine may submit many small BSP surface draw calls per frame. To avoid excessive `vkCmdDraw` overhead:
- Batch surfaces that share the same shader/texture/lightmap/pipeline state into a single draw call using multi-draw indirect (`vkCmdDrawIndexedIndirect`) if available, or manually merged draw ranges.
- Pre-sort the surface list by pipeline state before recording commands.

### 11.3 Shader Stages (Q3 Multi-Pass)

Quake 3 materials can define multiple rendering passes (stages). Implement multi-pass rendering by re-recording draw commands for each stage within the same render pass subpass, with appropriate blend state for each stage. Do **not** use multiple subpasses (the dependency complexity is not worth it for Q3's simple pass structure).

### 11.4 Fog

Render fog volumes as a separate pass after opaque geometry. Use depth test (`LESS_OR_EQUAL`, depth write off) and additive/src-alpha blending per the Q3 fog shader spec.

### 11.5 Skybox

Draw skybox after opaque geometry with depth write disabled and depth compare `LESS_OR_EQUAL`. Render as a cube with the skybox cubemap or the Q3 six-sided sky surfaces.

### 11.6 Gamma / Brightness

Q3 applies gamma via a table baked into texture uploads (overbrightening). Replicate the same table-based approach used in the OpenGL driver to ensure visual parity. Do **not** use swapchain HDR formats for this.

---

## 12. 2D / UI Rendering Path

- Switch to an orthographic projection for 2D rendering (typically called between `BeginScene`/`EndScene` or via a distinct 2D pipeline).
- Use the `2d` pipeline: depth test disabled, depth write disabled, alpha blending enabled.
- Batch consecutive `Draw2DQuad` / `DrawStretchPic` calls into the dynamic vertex ring buffer and flush when the pipeline/texture changes.

---

## 13. Screenshots (`ReadPixels`)

1. Create a host-visible `VkBuffer` of size `width × height × 4`.
2. After `EndFrame` submission, wait on the render fence.
3. Blit/copy the swapchain image (transition to `TRANSFER_SRC_OPTIMAL`, copy to buffer, transition back).
4. Map the buffer and return pixel data to the engine.
5. The engine owns format conversion (BGR→RGB, flip Y if needed) — check the other drivers to confirm.

---

## 14. Debug & Validation

- In debug builds, register `vkCreateDebugUtilsMessengerEXT` callback; route `ERROR` and `WARNING` severity messages to the engine's `Com_Printf` / log system.
- Apply `VK_EXT_debug_utils` object names to all long-lived Vulkan objects (pipelines, buffers, images) for readable RenderDoc captures.
- Insert `vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT` around major render phases (World, Entities, UI, etc.).
- Provide a `r_vulkanValidation` cvar (default off in release, on in debug) to toggle the validation layer at startup.

---

## 15. Configuration CVars

| CVar | Default | Description |
|---|---|---|
| `r_vulkanValidation` | `0` | Enable Vulkan validation layers |
| `r_vulkanVsync` | `1` | Present mode: 1 = FIFO (vsync), 0 = MAILBOX/IMMEDIATE |
| `r_vulkanAnisotropy` | `8` | Max anisotropy (1 = off) |
| `r_vulkanFrameOverlap` | `2` | Number of in-flight frame slots (1–3) |
| `r_vulkanDumpPipelines` | `0` | Log pipeline creation events |

---

## 16. Build System Integration

- Add a `USE_VULKAN_DRIVER` preprocessor define and corresponding build target/Makefile variable.
- Link against `vulkan-1.lib` / `libvulkan.so` dynamically (load via `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr`); do **not** link statically.
- Include VMA as a single-header library in the source tree (`third_party/vma/vk_mem_alloc.h`).
- SPIR-V shaders compiled as part of the build; add a `shaders/` subdirectory with a build rule invoking `glslangValidator`.
- The driver registers itself with the engine's driver registry using the same mechanism as the OpenGL and D3D11 drivers.

---

## 17. Testing & Validation Checklist

An agent completing this implementation must verify each item before marking the task done.

### Correctness
- [ ] All driver interface entry points implemented; no stubs left returning without action
- [ ] `r_drawWorld 0` disables world rendering without crash
- [ ] `r_drawEntities 0` disables entity rendering without crash
- [ ] All Q3 test maps load and render without visual corruption
- [ ] Multi-pass Q3 shaders (e.g., `textures/base_wall/proto_block`, animated shaders) render correctly
- [ ] Fog volumes match OpenGL reference
- [ ] Skybox renders without seams or Z-fighting
- [ ] UI, console, and HUD render correctly at all resolutions
- [ ] Screenshots produce correct pixel output

### Stability
- [ ] No validation layer errors or warnings at `ERROR`/`WARNING` severity in any test map
- [ ] Swapchain resize (window drag) does not crash or corrupt rendering
- [ ] Minimize/restore cycle recovers correctly
- [ ] Running for 10+ minutes in a multiplayer map produces no memory growth (check VMA stats)
- [ ] Shutdown path destroys all Vulkan objects without leaks (validation layer reports no objects alive at destroy)

### Performance
- [ ] Frame time at parity with OpenGL driver on a mid-range discrete GPU at 1080p
- [ ] No redundant pipeline state switches per frame (verify with RenderDoc)
- [ ] Pipeline cache hit rate > 95% after second launch (cold start is acceptable)

### Platform
- [ ] Builds and runs on Windows 10+ (Win32 surface)
- [ ] Builds and runs on Linux with X11 (XCB surface)
- [ ] Builds and runs on macOS 12+ via MoltenVK (Metal surface)

---

## 18. Agent Instructions & Working Conventions

- **Read before writing.** Before implementing any entry point, read its signature in the interface header and its implementation in both the OpenGL and D3D11 drivers. Understand what state it sets and what it is expected to produce.
- **Incremental commits.** Commit after each logical unit (e.g., init/shutdown, first triangle, texture upload, world geometry, UI). Each commit must leave the codebase in a buildable state.
- **No silent fallbacks.** If a required Vulkan feature or extension is absent, fail with a clear human-readable error message via the engine's error system. Do not silently skip functionality.
- **Match the OpenGL driver's behavior as the reference.** When in doubt about what a draw call or state change should produce visually, treat the OpenGL driver output as ground truth.
- **Do not change the interface header or the engine-side calling code.** All changes are confined to `renderer/vulkan/`.
- **Flag ambiguities.** If the interface contract is underspecified for a particular entry point, note it with a `// FIXME(vulkan): ambiguous — matches OpenGL behavior` comment and implement the OpenGL-matching behavior.