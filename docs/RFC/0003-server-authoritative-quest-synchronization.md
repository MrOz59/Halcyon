# RFC-0003 — Server-Authoritative Quest Synchronization

**Status:** Research Draft  
**Authors:** Halcyon Project Contributors

## Summary

This RFC outlines a long-term direction for coordinating Vanilla Quest state through Halcyon Contexts.

It is not an implementation-ready specification.

## Problem

Skyrim quests are executed locally through quest records, aliases, fragments, scenes, Papyrus scripts, and engine state.

Blindly broadcasting quest stages can:

- skip required scripts;
- break aliases;
- select incompatible branches;
- advance unrelated Players;
- regress progression after loading an older save;
- kill or disable critical actors globally.

## Proposed Direction

The server should coordinate supported quest state by Context.

Potential responsibilities:

- record observed quest state;
- detect supported transitions;
- reject stale regressions;
- scope narrative actor and object mutations;
- send typed client commands;
- maintain branch decisions;
- restore effective state after reconnect.

## Authority Levels

Quest synchronization may use several levels:

### Observation Only

Server records local quest state for diagnostics.

### Coordinated

Server accepts one Context leader or validated transition and forwards it.

### Server Validated

Server checks allowed stage transitions and Context membership.

### Server Authoritative

Server decides the accepted quest outcome and instructs clients.

Different quests may support different levels.

## Quest Profiles

Halcyon may require per-quest metadata profiles.

Example:

```yaml
quest: Skyrim.esm:0003372B
scope: party
stages:
  10:
    compatibleWith: [10, 20]
  20:
    irreversible: true
criticalActors:
  - Skyrim.esm:0003C57C
```

This format is illustrative.

## Client Quest Adapter

The client may need approved operations:

- read stage;
- set stage;
- display objective;
- complete objective;
- enable or disable Reference;
- apply actor life state;
- report alias resolution.

## Narrative Instances

When incompatible state cannot coexist safely, the server may create a Narrative Instance.

The instance may isolate:

- critical actors;
- quest objects;
- non-player Entities;
- or an entire cell.

## Initial Research Targets

Before implementation:

1. map existing STR quest synchronization;
2. identify common failure modes;
3. study quest stages, aliases, fragments, and scenes;
4. test one simple quest;
5. test one branching quest;
6. test one critical-NPC quest;
7. determine which operations are reversible;
8. determine whether a generic solution is realistic.

## Out of Scope for First Prototype

- full vanilla quest coverage;
- automatic arbitrary mod support;
- complete Papyrus server execution;
- automatic safe merge;
- general rollback.

## Risks

- quest stage numbers are not a linear progression model;
- SetStage may trigger irreversible fragments;
- aliases may differ;
- local saves may contain incompatible script state;
- clients may need reload or local repair;
- some quests may never be safely synchronizable.

## Acceptance Criteria for Research Phase

The RFC can advance from Research Draft when the project has:

- a map of current quest-sync code;
- reproducible desync tests;
- one successful Context-scoped quest prototype;
- documented unsupported cases;
- a proposed quest-profile format.

## Open Questions

- Who has progression authority in a Party?
- How is solo progress committed after joining a Party?
- Can a Player participate as a guest?
- How are mutually exclusive branches handled?
- Can quest fragments be replayed safely?
- When is native quest synchronization abandoned in favor of Runtime Quest UI?
