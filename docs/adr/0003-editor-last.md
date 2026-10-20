# 0003 — Build the Editor After the Runtime Vertical Slice

- **Status:** Accepted
- **Date:** 2026-10-19

## Context

A full editor is one of the largest and most tempting parts of an engine
project. Building it before the runtime can render, simulate, save, and load a
playable scene risks designing tools around unproven data and workflows.

## Decision

Defer the full editor to Phase 13. During earlier phases, ship a debug ImGui
overlay for logs, frame stats, culling, and component inspection. Use
hand-authored JSON scenes and command-line tools to drive the vertical slice.
Design asset and scene formats with editor needs in mind, but do not build
authoring UI until the runtime path is stable.

## Consequences

- Engineering effort goes into the game loop, renderer, physics, and content
  pipeline first.
- Content authoring is intentionally rough until the vertical slice works.
- The debug overlay and command-line tools must be good enough to diagnose
  runtime problems without an editor.
- Editor work starts from real runtime data and real content workflows.
