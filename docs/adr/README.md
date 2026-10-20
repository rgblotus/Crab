# Architecture Decision Records

Short, durable records of decisions that shape Crab's architecture. Add a new
record when a decision is costly to reverse or likely to be questioned later.

| ADR | Decision | Status |
|---|---|---|
| [0001](0001-vulkan-only.md) | Use Vulkan 1.3 as the only graphics API | Accepted |
| [0002](0002-ecs-with-explicit-hierarchy.md) | Use EnTT ECS with an explicit hierarchy component | Accepted |
| [0003](0003-editor-last.md) | Build the editor after the runtime vertical slice | Accepted |
| [0004](0004-dependency-policy.md) | Pin dependencies and keep the bootstrap small | Accepted |
| [0005](0005-3d-first.md) | Ship the 3D third-person path before 2D | Accepted |
| [0006](0006-rhi-facade-and-headless-path.md) | Keep Vulkan behind an RHI facade with a headless path | Accepted |

Format: `NNNN-short-title.md`, with Status, Context, Decision, Consequences.
