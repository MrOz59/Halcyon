# HTDS-003 — Goals

| Field | Value |
| --- | --- |
| Document ID | HTDS-003 |
| Title | Goals |
| Version | 0.1.0 |
| Status | Draft |
| Audience | All Contributors |

## 1. Purpose

This document defines the goals that guide Halcyon's architecture and development priorities.

These goals are intentionally broader than individual features. They describe the capabilities the platform is expected to provide and the constraints that implementations must respect.

When two implementation options conflict, the option that better satisfies the goals in this document SHOULD be preferred unless an accepted ADR explicitly states otherwise.

## 2. Primary Goals

### 2.1 Preserve Vanilla Cooperative Gameplay

Halcyon MUST preserve the ability for a small group of players to experience Skyrim's vanilla content together.

This includes:

- movement and remote-player replication;
- combat;
- equipment;
- inventory;
- spells and actor values;
- parties;
- vanilla quest progression;
- dialogue and interaction flows where technically feasible.

Existing working behavior from Tilted Evolution MUST NOT be removed without a documented replacement or migration path.

### 2.2 Introduce Progressive Server Authority

Halcyon MUST move toward a server-authoritative architecture.

The server SHOULD become the definitive authority for:

- Context membership;
- persistent multiplayer state;
- entity ownership;
- combat outcomes where practical;
- actor state;
- world changes;
- runtime quests;
- public events;
- PvP rules;
- rewards;
- persistence.

Authority MAY be migrated incrementally.

Legacy client-authoritative behavior MAY remain temporarily when replacing it would break compatibility or exceed the scope of the current implementation phase.

Every temporary authority exception SHOULD be documented.

### 2.3 Support Public and Private Servers

Halcyon MUST support both private cooperative sessions and persistent public servers.

Private servers SHOULD be able to favor simplicity and aggressive party-wide synchronization.

Public servers SHOULD support:

- multiple independent players;
- multiple parties;
- persistent state;
- optional PvP;
- narrative isolation;
- public events;
- administrative controls;
- recovery after restart or disconnect.

The architecture MUST NOT assume that every connected player shares one campaign state.

### 2.4 Isolate Narrative Progression

Actions performed by one player or party MUST NOT unintentionally advance, block, invalidate, or permanently alter the narrative progression of unrelated players.

Narrative state MUST be associated with a Context.

Examples include:

- personal quest progress;
- party quest progress;
- branch decisions;
- critical NPC deaths;
- quest-specific objects;
- dungeon state;
- scripted scenes.

When two narrative states are incompatible, Halcyon SHOULD isolate them through Context-aware replication, phasing, scoped change forms, or narrative instances.

### 2.5 Maintain a Shared Multiplayer World

Narrative isolation MUST NOT automatically imply complete player isolation.

Players SHOULD be able to coexist in the same physical world when their active narrative state does not require separation.

Depending on server rules, players MAY:

- see one another;
- communicate;
- trade;
- cooperate;
- duel;
- attack one another;
- join and leave parties;
- participate in public events.

Replication MUST consider both spatial relevance and Context relevance.

### 2.6 Support Runtime Multiplayer Content

The server MUST be able to create multiplayer activities that do not originate from a pre-authored vanilla quest.

The runtime quest system SHOULD support:

- public events;
- world bosses;
- bounty hunts;
- escorts;
- defense events;
- invasions;
- treasure hunts;
- contracts;
- dungeon objectives;
- faction events;
- seasonal content.

Runtime quest authority MUST remain on the server.

Clients SHOULD render objectives, markers, notifications, progress, timers, and rewards through native UI integrations.

### 2.7 Provide an Extensible Game-Mode Architecture

Halcyon MUST allow server-specific gameplay to be implemented without repeatedly modifying the multiplayer core.

The platform SHOULD provide stable extension points for:

- game modes;
- runtime quests;
- events;
- commands;
- rewards;
- factions;
- economy;
- PvP policies;
- persistence adapters;
- administrative integrations.

The low-level core SHOULD remain native C++.

High-level gameplay MAY use a managed or scripting language after the core boundaries are stable.

TypeScript is the current leading candidate but is not yet mandated.

### 2.8 Treat Linux as a First-Class Platform

Halcyon MUST support Linux development and server deployment.

The client MUST remain usable under supported Wine or Proton environments.

The project SHOULD provide:

- Linux build documentation;
- Linux CI for portable components;
- Proton-focused diagnostics;
- reproducible packaging;
- Steam Deck-compatible workflows where feasible;
- platform-neutral server dependencies.

Dependencies known to be fragile under Wine, such as embedded Chromium runtimes, SHOULD be avoided when a native alternative is practical.

Linux-first MUST NOT mean Windows-incompatible.

### 2.9 Preserve Protocol and Ecosystem Compatibility Where Practical

Halcyon SHOULD retain compatibility with existing Skyrim Together infrastructure when doing so does not block core architectural goals.

Compatibility concerns include:

- existing servers;
- existing clients;
- protocol versioning;
- existing mods;
- existing saves;
- existing deployment workflows.

Breaking changes MUST be versioned and documented.

The project MUST distinguish between:

- build identity;
- protocol compatibility;
- content compatibility;
- save compatibility;
- plugin compatibility.

### 2.10 Load and Understand Game Data Server-Side

The server SHOULD load sufficient ESM, ESP, and ESL metadata to understand the world it is validating.

This may include:

- forms;
- references;
- cells;
- actors;
- quests;
- factions;
- containers;
- doors;
- leveled lists;
- keywords;
- scripts;
- aliases.

Server-side game data SHOULD be used to validate FormIDs, ownership, entity types, cell membership, quest relevance, and mod compatibility.

Direct reuse of external parsers MUST comply with their licenses.

### 2.11 Provide Persistent World State

Public server state MUST survive process restarts.

Persistence SHOULD cover:

- player profiles;
- Contexts;
- party state;
- authoritative entities;
- scoped change forms;
- runtime quests;
- public events;
- bounty records;
- rewards;
- administrative actions;
- audit data.

Persistence SHOULD store deltas over base game data rather than duplicate entire worlds for every Context.

### 2.12 Support Safe Reconnection and Recovery

A client reconnecting after a crash, disconnect, death, or local save reload MUST be reconciled with the authoritative server state.

A stale client MUST NOT overwrite newer server state merely because it loaded an older save.

The server SHOULD provide:

- revision tracking;
- authoritative snapshots;
- delta recovery;
- Context restoration;
- party restoration;
- idempotent event handling where possible.

### 2.13 Provide Clear Diagnostics

Halcyon MUST prioritize diagnosability.

Errors SHOULD identify whether a failure originates from:

- protocol mismatch;
- mod mismatch;
- invalid game data;
- network connectivity;
- authentication;
- persistence;
- scripting;
- Context conflict;
- client integration;
- server validation.

The project SHOULD provide structured logs, useful error codes, and administrator-facing diagnostics.

### 2.14 Support Modded Servers

Halcyon SHOULD support modded gameplay when all required content is available and compatible.

The platform SHOULD eventually provide:

- mod manifests;
- load-order verification;
- plugin hash verification;
- clear mismatch reporting;
- server-required mod metadata;
- optional automatic launcher integration.

Halcyon MUST NOT claim that arbitrary single-player mods are automatically multiplayer-safe.

### 2.15 Keep the Core Maintainable

The architecture MUST avoid turning the server into a single tightly coupled subsystem.

Major responsibilities SHOULD have explicit boundaries, including:

- networking;
- world state;
- Contexts;
- replication;
- persistence;
- runtime quests;
- game data;
- scripting;
- administration.

A contributor SHOULD be able to work on one subsystem without understanding the entire codebase.

## 3. Secondary Goals

### 3.1 Administrative Tooling

Halcyon SHOULD eventually provide:

- server console commands;
- permissions;
- moderation;
- bans;
- audit logs;
- metrics;
- health endpoints;
- remote administration;
- optional web dashboards.

### 3.2 Economy and Social Systems

The platform MAY support:

- player trading;
- server currencies;
- guilds;
- factions;
- reputation;
- player housing;
- territory control;
- marketplaces.

These systems MUST be built as extensions unless they are required by the core architecture.

### 3.3 Horizontal Scalability

The architecture SHOULD avoid assumptions that permanently prevent:

- multiple server processes;
- regional shards;
- shared persistence;
- event workers;
- external matchmaking;
- cross-server services.

Full clustering is not an initial implementation requirement.

### 3.4 Testing and Simulation

Halcyon SHOULD provide automated test infrastructure for:

- protocol serialization;
- Context filtering;
- persistence;
- replication;
- runtime quests;
- game-data parsing;
- authority validation.

Headless simulation clients MAY be introduced to test multiplayer behavior without launching Skyrim.

## 4. Goal Priorities

When goals conflict, the default priority order is:

1. data integrity and player-progress safety;
2. security and authoritative consistency;
3. vanilla cooperative compatibility;
4. Linux and Proton reliability;
5. public-server scalability;
6. extensibility;
7. performance;
8. visual polish.

This order is not absolute. An accepted ADR MAY define a different priority for a specific subsystem.

## 5. Validation

A goal becomes meaningful only when it can be validated.

Each future subsystem specification SHOULD include:

- measurable acceptance criteria;
- failure cases;
- compatibility expectations;
- test strategy;
- migration implications.

The roadmap MUST NOT treat a subsystem as complete solely because an API or class exists.

## 6. Closing Statement

Halcyon's goals require balancing compatibility with architectural change.

The project will not become server-authoritative in one rewrite, nor will every Skyrim subsystem become perfectly deterministic.

Success depends on incremental migration, explicit ownership, Context-aware state, careful persistence, and a refusal to confuse temporary legacy behavior with the final design.
