# 0002 — Use EnTT ECS with an Explicit Hierarchy Component

- **Status:** Accepted
- **Date:** 2026-10-19

## Context

The engine needs a scene representation that is fast for gameplay systems,
supports a Godot-like tree in the editor, and serializes predictably. A pure
object graph is convenient for authoring but poor for cache-friendly iteration;
a pure flat ECS makes transforms and editor ergonomics awkward.

## Decision

Use EnTT as the storage and iteration layer. Represent parent/child links with
an explicit hierarchy component and maintain a flat `RenderWorld` extraction for
rendering. The editor and scene loader expose the hierarchy as a tree, while
gameplay systems iterate components directly.

## Consequences

- Systems are data-oriented and can run in parallel over component views.
- Transform propagation and reparenting are explicit, testable operations.
- Serialization must preserve entity GUIDs, not temporary ECS handles.
- The editor is a view over the same world, not a second scene model.
