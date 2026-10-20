# 0001 — Use Vulkan 1.3 as the Only Graphics API

- **Status:** Accepted
- **Date:** 2026-10-19

## Context

Crab targets Linux first and needs explicit control over synchronization,
memory, and modern GPU features (dynamic rendering, synchronization2, timeline
semaphores). Supporting multiple graphics APIs early would multiply RHI work,
testing surface, and shader-path complexity before the renderer has proven
itself.

## Decision

Implement a custom RHI over Vulkan 1.3 only. Do not add OpenGL, D3D12, or Metal
backends during the v1 path. Keep the RHI API backend-neutral enough that a
future backend is possible, but do not pay for one now.

## Consequences

- The renderer can use Vulkan 1.3 features directly and validate aggressively.
- Linux is the only supported platform for v1; portability is a later project.
- Every renderer feature must be testable through validation layers and GPU
  captures.
- The window layer creates a Vulkan surface from SDL3; SDL's GPU/render
  abstractions are not used for engine rendering.
