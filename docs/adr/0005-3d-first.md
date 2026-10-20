# 0005 — Ship the 3D Third-Person Path Before 2D

- **Status:** Accepted
- **Date:** 2026-10-19

## Context

Crab plans to support both 2D and 3D. The first game is a 3D third-person
action-adventure vertical slice. Splitting effort across 2D and 3D early would
delay the systems that prove the engine's core value: 3D rendering, character
movement, cameras, physics, navigation, animation, and 3D audio.

## Decision

Prioritize the 3D path through the first vertical slice. Defer the 2D renderer,
tilemaps, Box2D, sprite animation, and 2D lighting until after v1. Keep the core
architecture 2D-capable, but do not schedule 2D work against the first game's
critical path.

## Consequences

- Engine milestones are measured against a playable 3D slice, not feature
  checklists.
- The 2D renderer can reuse the RHI, asset database, scene model, and UI later.
- Some generic engine abstractions may need revision when 2D arrives.
- The go/no-go gate after Phase 7 is a playable gray-box 3D level.
