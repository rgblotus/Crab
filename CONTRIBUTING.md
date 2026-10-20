# Contributing to Crab

Crab is a long-lived engine project; keep changes small, measured, and easy to
review. The architecture plan and ADRs are the source of truth for direction.

## Build and test

```bash
./scripts/setup_ubuntu.sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Use `linux-headless` on machines without SDL desktop development packages, and
`linux-asan` or `linux-headless-asan` for sanitizer runs.

## Code style

- C++20, no compiler extensions.
- Types are `PascalCase`; functions and variables are `camelCase`; members are
  `m_camelCase`; files and directories are `snake_case`.
- Prefer RAII, `std::unique_ptr`, `std::string_view`, and value semantics.
- Keep public headers free of platform-specific types where practical; forward
  declare SDL and Vulkan handles when possible.
- Run `clang-format` before committing. CI runs `clang-format --dry-run`.
- Do not add a dependency without a pinned tag and an update to
  `THIRD_PARTY_NOTICES.md`.
- New subsystems need unit tests for pure logic and a smoke path for the
  runtime.

## Commits and decisions

- Keep commits focused; describe behavior, not just files changed.
- Record costly or surprising architectural choices in `docs/adr/`.
- Do not introduce editor-only assumptions into runtime code; the runtime must
  remain headless-testable.

## Phase discipline

The current track is 3D-first. The first vertical slice is the gate for
renderer, physics, navigation, animation, audio, and scripting work. Defer 2D
renderer, tilemaps, Box2D, and editor features until their scheduled phases.
