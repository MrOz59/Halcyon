# HTDS-000 — Preface

| Field | Value |
| --- | --- |
| Document ID | HTDS-000 |
| Title | Preface |
| Version | 0.1.0 |
| Status | Draft |
| Audience | Engine developers, gameplay developers, plugin authors, server administrators, and contributors |
| Language | English |

## 1. Purpose

The Halcyon Technical Design Specification defines the intended architecture, governing principles, and long-term technical direction of the Halcyon multiplayer platform.

The HTDS is not an expanded README and is not merely a description of the current source tree. It is an engineering specification. It exists to explain:

- what Halcyon is intended to become;
- which responsibilities belong to the client, server, plugins, and external tools;
- why major architectural decisions were selected;
- which compatibility and platform constraints are considered mandatory;
- how individual subsystems are expected to interact;
- how the project can evolve without losing architectural coherence.

Implementation SHOULD follow the specification. When implementation work demonstrates that the specified design is incomplete or incorrect, the specification MUST be revised deliberately rather than ignored.

## 2. Project origin

Halcyon begins from the Linux/Proton fork of Tilted Evolution, the codebase behind Skyrim Together Reborn.

That foundation already provides difficult and valuable capabilities:

- synchronization of players and remote actors;
- replication of movement, combat, equipment, inventory, spells, and actor values;
- integration with the Skyrim runtime;
- a working multiplayer protocol and dedicated server;
- party and social systems;
- a native ImGui interface that avoids the unstable CEF path under Wine;
- a launcher and injection strategy validated under Proton;
- compatibility with existing Skyrim Together Reborn servers.

Halcyon preserves this work instead of discarding it.

However, the long-term project is broader than a Linux compatibility fork. Halcyon intends to evolve the existing cooperative architecture into a server-authoritative multiplayer platform capable of supporting private cooperative sessions, persistent public servers, custom game modes, runtime quests, public events, PvP systems, and isolated narrative progression.

## 3. Project identity

Halcyon is not defined as an MMORPG.

Halcyon is a multiplayer platform on which different server experiences may be built.

A server may choose to provide:

- a mostly vanilla cooperative campaign;
- a private world for a small group;
- a public shared world;
- a roleplay environment;
- a survival game mode;
- a PvP-focused server;
- an MMO-lite experience;
- a server centered on custom events and runtime quests.

The core MUST provide reusable infrastructure rather than hard-code one final game mode.

## 4. Central design problem

Skyrim was designed as a local, single-player simulation. Each client naturally assumes that its save, quest VM, loaded cells, actors, scripts, and world changes represent the authoritative game state.

A multiplayer system that merely forwards those local changes can support cooperative play, but it develops structural problems as the number of players and length of a session increase:

- clients may disagree about quest stages;
- loading an older save may regress local world state;
- a death or permanent world change may affect unrelated players;
- one player's quest interaction may advance or block another player's quest;
- public servers cannot safely treat every narrative decision as globally permanent;
- the server cannot validate actions it does not understand;
- custom server-driven gameplay becomes difficult to implement cleanly.

Halcyon addresses this by separating the shared physical world from scoped narrative state and by moving authoritative decisions toward the server.

## 5. Architectural direction

Halcyon combines strengths from two existing approaches.

From Tilted Evolution, it retains:

- deep integration with the vanilla game;
- native client hooks;
- cooperative synchronization;
- the established protocol and entity systems;
- the Linux/Proton client work already completed.

From SkyMP, it studies and may adapt:

- server-side world representation;
- plugin-data parsing;
- authoritative entities and change forms;
- extensible game-mode systems;
- server-side scripting;
- persistent custom gameplay;
- runtime-generated activities.

Halcyon MUST NOT adopt a dependency merely because another project uses it. In particular, the SkyMP client stack based on Skyrim Platform and Chromium conflicts with Halcyon's Linux and native-client goals and is not part of the initial direction.

## 6. Context-oriented multiplayer

The defining architectural concept of Halcyon is the **Context**.

A Context is a logical scope that owns or selects state. Contexts allow different views of the narrative world to coexist on the same server.

Examples include:

- the global public world;
- a player's personal narrative progression;
- a party's shared campaign;
- a narrative instance for a branching quest;
- a runtime public event;
- an instanced dungeon;
- a server-created bounty contract.

This model supports a shared world without requiring all players to share every quest decision.

A player outside a party may see and fight another player when PvP is enabled while remaining unable to alter that player's quest actors, objectives, containers, or scripted progression.

## 7. Server authority

The target architecture is server-authoritative.

Clients report observations and request actions. The server validates those requests, updates authoritative state, and replicates accepted results to eligible recipients.

This does not imply that every Skyrim engine subsystem can immediately move to the server. Migration will be incremental. The HTDS will distinguish between:

- fully server-authoritative state;
- server-validated client state;
- temporarily client-authoritative legacy behavior;
- cosmetic state that does not require authority.

Temporary compatibility decisions MUST NOT be mistaken for the final architecture.

## 8. Linux as a first-class platform

Halcyon treats Linux as a first-class client-development and server-deployment platform.

The Skyrim process remains a Windows executable running through Wine or Proton, but Halcyon's own design MUST avoid unnecessary dependencies that are known to be fragile in that environment.

The project SHOULD prefer:

- native C++ integrations over embedded web runtimes;
- ImGui and game-native UI integrations over CEF when practical;
- portable server dependencies;
- reproducible Linux CI;
- documented Proton behavior;
- Steam Deck compatibility where technically feasible.

Windows compatibility remains important. Linux-first does not mean Linux-only.

## 9. Extensibility

The multiplayer core should provide infrastructure. Server-specific gameplay should be implementable without repeatedly forking the engine.

Long-term extension points include:

- runtime quests;
- world events;
- bounty systems;
- factions and guilds;
- economy rules;
- custom replication policies;
- administrative commands;
- persistence adapters;
- server game modes.

The core will remain C++ where low latency, safety, direct engine integration, or high-frequency processing requires it. A higher-level scripting layer, likely TypeScript, may host game-mode and event logic after the authoritative core is stable.

## 10. Document organization

The HTDS is divided into five main volumes:

1. **Foundation** — vision, goals, scope, comparisons, principles, and terminology.
2. **Core Architecture** — server, client, networking, world state, entities, replication, persistence, and lifecycle.
3. **Gameplay Systems** — Contexts, narrative instances, quest synchronization, runtime quests, PvP, events, and bounty systems.
4. **SDK and Extensibility** — C++, TypeScript, Papyrus integration, plugins, and mod compatibility.
5. **Infrastructure** — storage, deployment, administration, security, observability, performance, and operations.

Supporting materials are kept in ADR, RFC, and Research collections.

## 11. Relationship with ADRs, RFCs, and research

The HTDS describes accepted intended architecture.

An RFC proposes a change that is still being discussed.

An Architecture Decision Record captures a decision and its consequences after acceptance.

A research document records evidence, experiments, comparisons, and uncertainties without automatically making a project decision.

The expected flow is:

```mermaid
flowchart LR
    Research --> RFC
    RFC --> ADR
    ADR --> HTDS
    HTDS --> Implementation
    Implementation --> Validation
    Validation --> Research
```

Small and obvious implementation details do not require this full process. Major architecture changes SHOULD follow it.

## 12. Living specification

The HTDS is versioned with the source code and evolves with the project.

It is expected to contain incomplete sections during early development. Incompleteness must be visible through status markers rather than hidden behind authoritative language.

Every document should identify its status:

- **Draft** — actively designed and not yet stable;
- **Proposed** — considered ready for review;
- **Accepted** — approved as intended architecture;
- **Implemented** — accepted and substantially represented in code;
- **Deprecated** — retained for history but no longer current;
- **Superseded** — replaced by another document.

## 13. Closing statement

Halcyon's goal is not merely to synchronize several independent Skyrim sessions.

Its goal is to provide a modern multiplayer foundation that preserves the strengths of Skyrim Together Reborn, supports Linux reliably, enables server-authoritative systems, isolates incompatible narrative state, and allows communities to build multiplayer experiences beyond the original campaign.

The HTDS is the blueprint for that work.
