# First Game — 3D Third-Person Vertical Slice

**Working codename:** `Hermit` (rename freely)
**Setting/theme:** TBD — choose one that supports a compact, readable level (ruins, cave, research facility, small island). Theme must not add systems (no swimming, climbing, vehicles).
**Genre:** compact third-person action-adventure / exploration slice
**Camera:** over-the-shoulder spring arm with collision; optional aim mode later
**Core loop:** explore a small level → avoid or fight one enemy type → collect a key → open the exit → reach the checkpoint
**Session length:** 15–30 minutes
**Target:** 1080p / 60 fps on a GTX 1660 / RX 580 class GPU, Linux

This document is the **customer specification** for the engine. When the engine plan and this document disagree about priority, this document wins. When this document asks for something the engine cannot do, we fix the engine API rather than hacking the game.

---

## 1. Why This Game First

A third-person 3D game is the right first customer because it forces the engine to solve the systems that are hardest to retrofit:

- PBR materials, real-time shadows, IBL, sky, fog, and a correct color pipeline.
- A character controller that feels good on slopes, steps, and moving platforms.
- A third-person camera with collision, smoothing, and input latency low enough to play.
- Skeletal animation with blending, events, and root motion.
- Navigation for enemies (navmesh or a deliberate simpler alternative).
- 3D spatial audio and a UI layer that works with keyboard, mouse, and gamepad.
- An asset pipeline that imports a real character, environment kit, animations, and audio.

It also avoids the 2D renderer, tilemaps, Box2D, 2D animation, and pixel-perfect camera work in v1. Those are valuable, but they do not validate the 3D pipeline. They move to post-v1.

---

## 2. Scope: What Ships in the Slice

### 2.1 Must have

| Area | Slice requirement |
|---|---|
| Level | One compact environment: a small exterior hub or a short dungeon, ~10–15 minutes of traversal, gray-box first then one art kit |
| Player | Third-person character: walk, run, jump, light attack, interact, take damage, die/respawn |
| Camera | Spring arm, collision, mouse/gamepad look, zoom, camera shake on hit; lock-on is optional |
| Enemy | One enemy type: idle/patrol/chase/attack/hit/death; simple navigation; telegraphed attack |
| Objective | One key/pickup, one locked exit, one checkpoint, win/lose states |
| Animation | Idle, walk, run, jump/fall, attack, hit, death; blend by speed; events for footsteps/attack windows |
| Physics | Character controller, capsule collisions, triggers, raycasts for camera and interaction |
| Audio | Footsteps, attack/hit, enemy vocalization, ambient loop, music loop, UI sounds; 3D attenuation |
| UI | Main menu, HUD (health/objective/interaction prompt), pause menu, settings (resolution, volume, sensitivity) |
| Input | Keyboard + mouse and gamepad, rebindable core actions |
| Lighting | One outdoor daylight setup or one indoor mood; real-time shadows; IBL/sky; fog |
| VFX | Hit spark, dust on landing, simple enemy death effect |
| Performance | 1080p/60 on target GPU; stable frame time; no hitching on first interaction |
| Packaging | Runs from a clean Ubuntu 24.04 install without the Vulkan SDK |

### 2.2 Nice to have (only if the slice is already fun)

- Lock-on targeting.
- Dodge roll with i-frames.
- A second enemy variant.
- A short dialogue/interaction line.
- Checkpoint save (resume position/health).
- One collectible lore item.
- A simple boss arena at the end.

### 2.3 Explicitly out of scope

Open world, quests, inventory, crafting, dialogue trees, cutscenes, multiple levels, mounts, climbing, swimming, vehicles, multiplayer, character creator, weather, day/night cycle, terrain, water, 2D mode, mobile, console, mod support, localization beyond English (but all strings externalized).

---

## 3. Content Budget

Keep the content small enough that the engine remains the bottleneck, not art.

| Asset | Budget |
|---|---|
| Player character | 1 rigged mesh, 3–5 materials, 7–9 animations |
| Enemy | 1 rigged mesh, 2–3 materials, 6–8 animations |
| Environment kit | 1 modular kit: floors, walls, stairs, railings, 10–15 props |
| Sky/lighting | 1 HDRI or procedural sky, 1 sun configuration |
| Audio | 15–25 SFX, 1 ambient loop, 1–2 music loops |
| UI | 1 font, 1 theme, 3–5 screens |
| VFX | 3–5 simple effects |

Use gray-box geometry and capsule characters until the gameplay loop works. Art passes come after the slice is fun. If the schedule slips, cut content, not the camera/character feel.

### 3.1 Feel targets (starting values, tune by playtest)

| Parameter | Starting value |
|---|---|
| Walk / run speed | 3.0 / 6.0 m/s |
| Acceleration / deceleration | 25 / 30 m/s² |
| Jump height / gravity | 1.5 m / −18 m/s² (game gravity, not world gravity) |
| Coyote time / jump buffer | 0.1 s / 0.1 s |
| Character turn rate | 720°/s (camera-relative movement) |
| Camera distance / shoulder offset | 4.0 m / 0.6 m |
| Camera collision radius | 0.3 m |
| Camera smoothing | 12–15 Hz critically damped |
| Pitch clamp | −60° to +45° |
| Attack wind-up / active / recovery | 0.2 s / 0.1 s / 0.3 s |
| Enemy telegraph | 0.5–0.8 s |
| Input-to-photon target | < 100 ms (measure, don't assume) |

Movement is camera-relative, with acceleration and a turn-toward-movement model. The camera leads slightly, never snaps, and pulls in on collision. These values are not sacred; the **feel** is.

---

## 4. Engine Requirements Mapped to the Game

| Game need | Engine system | Notes |
|---|---|---|
| Third-person view | Camera component + spring arm controller | Camera collision ray/sphere cast; smoothing; input latency budget |
| Character movement | Jolt `CharacterVirtual` + `CharacterController` component | Slope/step/ground snap; moving platforms; coyote time in game code |
| Combat | Animation events + physics queries | Hitboxes via shape casts; no separate combat framework in v1 |
| Enemy behavior | Lua/C++ state machine + navigation | Patrol/chase/attack/death; keep one enemy type |
| Enemy movement | Recast & Detour navmesh | Bake at import/build; query paths; agents follow corridors |
| Environment | glTF import + PBR materials + CSM/IBL | One kit; modular pieces; lightmap/baked GI later |
| Character animation | ozz + animation state machine | Locomotion blend space, attack/hit/death transitions, root motion optional |
| Animation retargeting | ozz offline tools + rest-pose mapping | Mixamo/asset packs; validate skeleton compatibility before content work |
| Foot placement | Two-bone IK | Only if it improves the slice; otherwise skip |
| Camera feel | Camera shake, FOV kick, smoothing | Tune early; add debug graphs |
| Audio | 3D spatial sources + buses | Footsteps/attacks routed to SFX; music/ambience to Music/Ambient |
| Interaction | Raycast + UI prompt | One interaction type is enough |
| Menus/HUD | RmlUi + gamepad navigation | Main menu, pause, settings, HUD |
| Save | Checkpoint state (optional) | Position/health/objective; full save system post-slice |
| Debugging | ImGui overlay + debug draw + Tracy | Character state, camera, navmesh, physics, frame stats |
| Packaging | Cooked assets + runtime + game module | One `.crabpak`, launcher, license screen |

### 4.1 Navigation: the missing engine system

A third-person game with enemies needs navigation. The generic engine plan did not include it; add it now:

- **Recast & Detour** (zlib license): navmesh baking, query, path following.
- Bake from level geometry at import/build time; store navmesh as an asset.
- Agent component: radius, height, speed, acceleration, avoidance, area costs.
- Queries: nearest point, random point, raycast, path corridor.
- Off-mesh links (ladders/jumps) are post-slice.
- Debug draw: navmesh polygons, agent paths, areas.

**Fallback if navigation blows the schedule:** waypoint graph + steering + obstacle avoidance. It saves 2–3 weeks but produces visibly limited enemies. Prefer Recast if the slice has corridors or rooms.

---

## 5. 3D-First Phase Order (Selected Track)

This supersedes the generic phase order in `ENGINE_PLAN.md`. The engine architecture is unchanged; the order and scope are game-driven.

| Phase | Focus | Estimate | Gate |
|---:|---|---:|---|
| 0 | Bootstrap, CMake, CI, logging, SDL3 window | 1–2 weeks | Build + window |
| 1 | Platform, input, jobs, time, events | 2–3 weeks | Fixed-step loop + profiler |
| 2 | Vulkan RHI bring-up + ImGui | 3–4 weeks | Triangle + ImGui, zero validation errors |
| 3 | Render graph + minimal 3D + camera | 3–4 weeks | Textured 3D mesh, depth, resize-safe |
| 4 | Scene/ECS, assets, serialization | 4–6 weeks | glTF scene load/save round-trip |
| 5 | 3D PBR + shadows + IBL + sky + post | 5–6 weeks | Lit PBR scene at 1080p/60 |
| 6 | Jolt physics + character controller + navigation | 6–8 weeks | Character on slopes/steps; enemy paths |
| 7 | 3D animation + state machine + events | 4–5 weeks | Locomotion blend + attack/hit/death |
| 8 | Audio | 2–3 weeks | Music/SFX/3D audio + buses |
| 9 | Scripting + gameplay framework | 3–4 weeks | Player/enemy logic in Lua; hot reload |
| 10 | Runtime UI (RmlUi) | 3–4 weeks | Menu/HUD/pause/settings + gamepad |
| 11 | 3D third-person vertical slice | 4–6 weeks | 15–30 min playable, 1080p/60 |
| 12 | Tools/polish: particles, checkpoint, profiler, docs | 3–5 weeks | Content-complete slice |
| 13 | Editor MVP | 6–8 weeks | Create/save/play a scene |
| 14 | Hardening + packaging | 4–6 weeks | Clean-machine packaged build |

**Sum:** ~53–74 weeks optimistic; **18–24 months realistic** full-time. The slice (Phase 11) lands around month 10–14, not month 6. That is the price of a 3D third-person game.

### 5.1 What moved

| Generic plan | Selected 3D-first plan |
|---|---|
| Phase 5: 2D renderer + text | Removed from v1; 2D is post-v1 |
| Phase 10: 3D PBR | Moved to Phase 5 |
| Phase 6: Physics 2D/3D | Phase 6 is 3D Jolt + navigation; Box2D deferred |
| Phase 7: Animation 2D/3D | Phase 7 is 3D only; sprite animation deferred |
| Phase 11: UI | Moved to Phase 10, before the slice |
| Phase 14: vertical slice | Moved to Phase 11; final phase is hardening |

The text stack still matters for UI and world labels. If RmlUi covers UI text, the custom HarfBuzz world-text path can be post-slice; do not let it block the game.

---

## 6. Milestones

| Milestone | Target | What exists |
|---|---|---|
| M0 | Week 2 | Window, logs, CI, build script |
| M1 | Week 6 | Vulkan device/swapchain, ImGui, shader hot reload |
| M2 | Week 10 | Render graph, textured 3D mesh, camera, debug draw |
| M3 | Week 14 | glTF scene import, ECS scene, save/load |
| M4 | Week 20 | PBR environment with shadows, IBL, sky, post |
| M5 | Week 28 | Gray-box level + capsule character controller + third-person camera |
| M6 | Week 35 | Animated character + locomotion/attack state machine |
| M7 | Week 40 | Enemy with navigation, combat, hit/death |
| M8 | Week 43 | Audio, HUD placeholder, interaction |
| M9 | Week 48 | Lua gameplay, checkpoint, win/lose loop |
| M10 | Week 52 | Menus, HUD, pause, settings, gamepad |
| M11 | Week 58 | Vertical slice complete, 15–30 min playthrough |
| M12 | Week 63 | Polish, particles, docs, performance pass |
| M13 | Week 71 | Editor MVP |
| M14 | Week 77 | Packaged release build |

These are planning targets, not commitments. If M5 slips, cut content and lock-on, not the camera/character feel.

### 6.1 First playable prototype (after Phase 7)

A gray-box arena with:

- A capsule character that walks, runs, jumps, and turns responsively.
- A third-person camera that never clips through walls.
- One enemy that patrols, sees the player, chases, attacks, and dies.
- One attack, one hit reaction, one death.
- Debug UI for state, velocity, camera distance, nav path.

This prototype is the **go/no-go gate**. If the character and camera do not feel good here, more rendering features will not save the game.

### 6.2 Vertical slice (after Phase 11)

The full 15–30 minute experience with one environment, one enemy type, one objective, menus, HUD, audio, checkpoint, and win/lose states. This is the portfolio piece and the engine's proof.

---

## 7. Acceptance Criteria

### 7.1 Gameplay

- [ ] Player can start a new game, play for 15–30 minutes, win or lose, and restart without relaunching.
- [ ] Character walks/runs/jumps with responsive input; no stuck-on-geometry or slope sliding bugs.
- [ ] Third-person camera follows smoothly, avoids walls, and does not jitter or clip.
- [ ] Attack has a readable wind-up, hit window, and recovery; hits register reliably at 60 fps.
- [ ] Enemy patrols, detects, chases via navmesh, attacks, takes hits, and dies.
- [ ] Interaction prompt appears only when in range and facing the target.
- [ ] Checkpoint respawns the player with correct health/objective state.
- [ ] Win/lose states are clear and restartable.

### 7.2 Presentation

- [ ] 1080p/60 on the target GPU with stable frame times; no first-interaction hitches.
- [ ] PBR materials, shadows, IBL, sky, and fog read correctly; no obvious color/gamma errors.
- [ ] Animations blend without pops; footsteps and attack events line up.
- [ ] 3D audio attenuates/panes correctly; music and SFX buses are balanced.
- [ ] HUD/menu readable at 1080p and 4K; gamepad navigation works.
- [ ] Hit VFX and camera shake are present but not nauseating.

### 7.3 Engineering

- [ ] Zero Vulkan validation errors in debug.
- [ ] No crash in a 1-hour soak; no asset/body/audio handle leaks.
- [ ] Hot reload of Lua and assets works during play; C++ module reload preserves the world.
- [ ] Packaged build runs on a clean Ubuntu 24.04 machine.
- [ ] Frame/CPU/GPU stats and Tracy captures available.
- [ ] All unit/integration/golden tests pass; scene round-trip stable.
- [ ] Third-party notices and engine license included.

---

## 8. Risks Specific to This Game

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Third-person camera feels bad | High | High | Prototype camera before combat; collision sphere cast; expose tuning values; test with mouse and gamepad |
| Character controller feels floaty/sticky | High | High | Use Jolt `CharacterVirtual`; tune early; add coyote time/jump buffer in game code; debug graphs |
| Navigation integration slips | Medium | High | Timebox Recast; fallback to waypoint steering; keep enemy count low |
| Animation/root motion integration | Medium | High | Use ozz; start with in-place locomotion; add root motion only for attacks if needed |
| Combat feel | Medium | High | One attack, clear timing, hit-stop, camera shake, audio; no combo tree in v1 |
| Level design takes longer than code | High | Medium | Gray-box; one kit; 10–15 minutes of traversal; reuse modular pieces |
| Scope creep into RPG/open world | High | High | Hard cut list; one enemy, one objective, one level |
| Asset quality/distraction | Medium | Medium | Use asset packs with clear licenses; capsule/gray-box until fun |
| Input latency/frame pacing | Medium | High | Clamp frames in flight; measure input-to-photon; avoid TAA/upscaling initially |
| Camera + UI input conflicts | Medium | Medium | Explicit input contexts (gameplay/menu); cursor capture rules |
| Solo burnout | High | High | Milestones, playable builds, record GIFs, take breaks |

---

## 9. Cut List (Use Without Guilt)

If the schedule slips, cut in this order:

1. Lock-on targeting.
2. Dodge roll and second attack.
3. Second enemy variant and boss.
4. Checkpoint save/load (keep respawn).
5. Lore collectible and dialogue line.
6. Foot IK and camera FOV kick.
7. Extra VFX and ambient detail.
8. Indoor/outdoor second lighting mood.
9. Custom character mesh (use a licensed asset).
10. Editor polish (ship with JSON scenes + debug tools if needed).

Do **not** cut: camera collision, character responsiveness, attack readability, navmesh basics, 60 fps target, packaged build, or the 15-minute playthrough. Those are the slice.

---

## 10. Definition of Done for the Slice

A stranger can install the build on Ubuntu 24.04, launch it, understand the controls, play for 15–30 minutes, win or lose, and want to play again. The build is stable, runs at 1080p/60 on the target GPU, and every major engine system used by the game is documented and tested.

That is the point at which the engine has earned the right to grow into a general-purpose 2D/3D engine.
