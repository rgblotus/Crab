# 0004 — Pin Dependencies and Keep the Bootstrap Small

- **Status:** Accepted
- **Date:** 2026-10-19

## Context

Game engines accumulate native dependencies quickly. Unpinned dependencies make
builds non-reproducible; vendoring everything makes upgrades and security review
painful; adding every future library on day one slows the bootstrap and obscures
which components are actually exercised.

## Decision

Use CMake `FetchContent` with exact tags for upstream dependencies. Keep the
Phase 0 set to SDL3, spdlog, GLM, and Catch2. Add Vulkan libraries, physics,
animation, audio, text, and editor dependencies only when their phase begins.
Allow system packages through explicit `CRAB_USE_SYSTEM_*` options where useful.
Tracy is opt-in via `CRAB_ENABLE_TRACY`.

## Consequences

- Clean clones are reproducible but require network access on first configure.
- Dependency upgrades are deliberate and recorded in `THIRD_PARTY_NOTICES.md`.
- CI builds only what the current phase needs, keeping feedback fast.
- A future dependency lock or vendored mirror can be added without changing the
  module boundaries.
