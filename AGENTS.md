# AGENTS.md — Halcyon Autonomous Agent Instructions

This file applies to autonomous and semi-autonomous coding agents working in the
Halcyon repository.

It is also useful to human contributors because it summarizes the project's
operating rules.

---

## 1. Mission

Halcyon evolves the Tilted Evolution / Skyrim Together Reborn Linux fork into a
context-oriented, progressively server-authoritative multiplayer platform.

The project intends to preserve the strongest existing functionality while adding:

- persistent public-server support;
- isolated personal and Party narrative state;
- Context-aware replication;
- server-owned Runtime Quests and public events;
- extensible game modes and plugins;
- first-class Linux and Proton support.

The repository is not a clean-room rewrite.

The current implementation is mostly Tilted Evolution plus Linux/Proton work.
Much of the HTDS describes a target architecture that has not yet been built.

---

## 2. Mandatory Reading Order

Before performing architectural or cross-cutting work, inspect:

1. `CONTRIBUTING.md`
2. `docs/HTDS/README.md`
3. `docs/HTDS/Volume-1-Foundation/`
4. the relevant subsystem in later HTDS volumes;
5. `docs/ADR/`
6. `docs/RFC/`
7. relevant files under `docs/Research/`
8. the current source implementation.

For a narrow bug fix, reading only the relevant architecture and source may be
sufficient.

Do not infer architecture only from directory names.

---

## 3. Source of Truth and Document Roles

Use this hierarchy:

### HTDS

Describes accepted intended architecture.

### ADR

Records an accepted decision and its consequences.

### RFC

Describes a proposal, research draft, or prototype plan that is not yet final.

### Research

Records evidence, source mapping, experiments, and unresolved questions.

### Source code

Describes what currently runs.

When these disagree:

- do not silently force the code to match speculative architecture;
- do not silently rewrite documentation to match legacy behavior;
- identify whether the difference is intentional migration;
- update the appropriate documents;
- create an ADR when a major direction changes.

---

## 4. Critical Truthfulness Rule

Never claim that a system is implemented unless the code actually implements it.

Much of the current documentation is marked:

- Conceptual;
- Draft Skeleton;
- Research Draft;
- Pre-Implementation.

Preserve those labels until acceptance criteria are met.

Do not invent:

- APIs;
- classes;
- directories;
- protocol messages;
- tests;
- benchmark results;
- external-project behavior;
- implementation status;
- source citations.

When uncertain, say that the fact is unverified and inspect the code or relevant
primary source.

---

## 5. Core Architectural Direction

### Preserve the Tilted Evolution foundation

The existing client, synchronization, protocol, launcher, and Linux work are
valuable.

Prefer incremental adaptation over replacement.

### Contexts are the primary isolation mechanism

New systems involving scoped multiplayer state should be evaluated through
Contexts.

Contexts may represent:

- Global state;
- Personal state;
- Party state;
- Narrative instances;
- Dungeon instances;
- Runtime Quests;
- Public Events;
- server-defined scenarios.

Do not create parallel state-scope mechanisms without documenting why Contexts are
insufficient.

### Shared world, independent stories

Players in different narrative states should remain visible and interactive when
safe.

Do not equate Context separation with complete Player isolation.

### Progressive server authority

Move authority toward the server subsystem by subsystem.

Do not require a complete rewrite before delivering value.

New persistent reward systems should be server-authoritative from the start.

### Base data plus scoped deltas

Prefer:

```text
base plugin data
+ global authoritative state
+ Context-scoped state
= effective world view
```

Avoid full world duplication unless a bounded instance requires it.

### Native client, server-driven results

Keep the client responsible for Skyrim integration and presentation.

Keep durable outcomes, rewards, Runtime Quests, Context membership, and public-server
rules on the server.

---

## 6. Current Non-Implementation Warning

The following target systems are not currently implemented as specified:

- generalized Context service;
- Context-aware narrative replication;
- authoritative World State;
- scoped persistent Change Forms;
- Runtime Quest framework;
- stable plugin/game-mode host;
- TypeScript game-mode runtime;
- full Game Data Service;
- server-authoritative Vanilla Quest synchronization;
- Context conflict resolver;
- narrative instances;
- stable Halcyon SDK.

Do not write code that assumes these services already exist.

Introduce the smallest interface needed by the current prototype.

---

## 7. Preferred Work Pattern

For substantial work:

1. map current code;
2. document current behavior;
3. identify the smallest architectural boundary;
4. define a prototype;
5. add diagnostics;
6. implement behind a safe boundary or feature flag;
7. test;
8. record results;
9. update RFC/ADR/HTDS;
10. only then generalize.

Avoid implementing a broad framework before one concrete use case proves it.

---

## 8. First Context Prototype

RFC-0001 is the current reference prototype.

The minimal scenario is:

1. two Players are in the same cell;
2. both Players remain visible;
3. each Player has a Personal Context;
4. one quest-critical Actor has a Context-scoped life state;
5. Player A kills the Actor;
6. only A's Context stores and receives the death;
7. Player B continues to see the Actor alive;
8. PvP between A and B still works;
9. state survives server restart and reconnect.

Do not expand the first prototype to full quest synchronization.

---

## 9. Runtime Quest Direction

Server-created gameplay should generally use Runtime Quests rather than attempting
to generate native `TESQuest` records dynamically.

Runtime Quests are server-owned.

The server decides:

- creation;
- participants;
- objective progress;
- completion;
- expiry;
- contribution;
- rewards.

The client displays state and submits Intents or Observations.

Do not let the client declare completion or grant durable rewards.

---

## 10. Vanilla Quest Synchronization Warning

Skyrim quests are not simple monotonic stage counters.

Do not assume:

- a higher stage is always later or compatible;
- `SetStage` safely reproduces all prior logic;
- aliases match across clients;
- fragments can be replayed;
- scenes can be restarted safely;
- arbitrary modded quests can be synchronized generically;
- quest rollback is safe.

Before modifying quest synchronization:

- inspect existing STR behavior;
- identify fragments, aliases, scenes, and Actor dependencies;
- document supported and unsupported cases;
- prefer a narrow quest profile over broad assumptions.

---

## 11. Linux and Proton Rules

Linux and Proton are first-class constraints.

For client or launcher work, inspect:

- Wine path behavior;
- prefix discovery;
- process creation;
- injection;
- high-ASLR behavior;
- raw input;
- focus transitions;
- D3D11 hooks;
- Steam launch methods;
- Vortex launch behavior;
- logging without a console.

Do not add mandatory CEF or Chromium dependencies to the Linux path without an
accepted architectural decision and validated Proton prototype.

Preserve the native ImGui path.

Do not assume a fixed `0x140000000` image base.

---

## 12. Code Change Rules

### Do

- keep changes focused;
- preserve complete existing behavior unless intentionally changing it;
- follow nearby style;
- use explicit ownership;
- validate external input;
- reject stale revisions;
- add useful logs;
- add tests when practical;
- update status documentation honestly;
- keep compatibility adapters temporary and visible.

### Do not

- perform unrelated mass formatting;
- rename broad directory trees only for aesthetics;
- delete legacy code before callers migrate;
- invent an abstraction without a concrete use case;
- hide architectural changes inside a bug-fix PR;
- use raw client claims for durable rewards;
- bypass Contexts for scoped state without justification;
- add unrestricted script access to internal pointers or storage.

---

## 13. Repository Structure Guidance

Existing Tilted Evolution code remains in its current locations until deliberate
migration.

New Halcyon-specific work may live under:

```text
halcyon/
├── server/
├── client/
├── sdk/
└── tools/
```

Do not move existing code into this layout merely to make the tree look cleaner.

A move is justified when:

- a real module boundary exists;
- includes and build rules are updated;
- tests cover the migration;
- the old path is removed safely;
- documentation reflects the new ownership.

---

## 14. Protocol Work

Before adding or changing protocol messages:

- inspect existing serialization conventions;
- separate protocol version from build identity;
- define direction;
- define reliability;
- define authority semantics;
- define validation;
- define capability requirements;
- define backward compatibility;
- define structured errors;
- define revision behavior;
- update protocol documentation.

Prefer capability negotiation for optional Halcyon features.

Do not silently send Context-dependent state to legacy clients.

---

## 15. Persistence Work

Persistent operations must consider:

- revisions;
- idempotency;
- transaction boundaries;
- crash recovery;
- schema migration;
- plugin-manifest compatibility;
- duplicate rewards;
- Context cleanup;
- audit requirements.

Critical durable operations should not report success before the required commit.

A local Skyrim save may be an import source, but must not overwrite newer
authoritative server state after import.

---

## 16. Plugin and Script Work

No stable Plugin Host exists yet.

When designing one:

- keep the native core authoritative;
- expose constrained APIs;
- use permissions;
- isolate failures;
- version APIs;
- namespace plugin storage;
- avoid unrestricted SQL;
- avoid unrestricted arbitrary client commands;
- document lifecycle;
- record external-code licenses.

TypeScript is a candidate, not an accepted implementation requirement.

Do not add a scripting runtime before a concrete prototype demonstrates its need.

---

## 17. Testing Expectations

Select validation appropriate to the change.

Possible checks:

- build;
- formatting;
- unit tests;
- serialization round trips;
- stale-revision rejection;
- two-client replication;
- Party behavior;
- Context isolation;
- reconnect;
- server restart;
- persistence recovery;
- Proton startup;
- UI interaction;
- log inspection;
- malformed packet rejection.

If testing is not possible, explicitly state what remains untested.

Never fabricate test output.

---

## 18. Research Rules

When studying STR, SkyMP, SKSE, Papyrus, or other projects:

- prefer primary sources;
- cite exact repositories, files, commits, issues, or official documentation;
- distinguish source observation from inference;
- verify component licenses before reuse;
- record uncertainties;
- do not copy code merely because architecture is useful.

SkyMP is a server-architecture reference, not the planned Halcyon client stack.

---

## 19. Documentation Update Rules

Update documentation when a change affects:

- authority;
- Context behavior;
- protocol;
- persistence;
- lifecycle;
- plugin APIs;
- quest synchronization;
- Linux architecture;
- public-server security.

Use:

- Research for evidence;
- RFC for proposal;
- ADR for accepted decision;
- HTDS for intended architecture.

Do not add an ADR for every small implementation detail.

---

## 20. Implementation Status Vocabulary

Use only accurate labels:

- Conceptual;
- Research Draft;
- Draft Skeleton;
- Prototype;
- Partially Implemented;
- Implemented;
- Validated;
- Legacy;
- Transitional;
- Deprecated;
- Superseded.

An interface stub is not an implemented subsystem.

A successful one-off test is not full validation.

---

## 21. Autonomous Agent Stop Conditions

Stop and request human review before:

- deleting large working subsystems;
- changing protocol compatibility broadly;
- rewriting persistence schemas destructively;
- adopting external GPL/AGPL code without license review;
- changing project license;
- adding mandatory new runtime dependencies;
- force-pushing;
- publishing releases;
- changing secrets or credentials;
- modifying CI permissions;
- changing security-sensitive authentication;
- claiming a major architecture milestone is complete.

When uncertain, prefer a draft RFC or a small isolated prototype.

---

## 22. Required Agent Report

At the end of substantial work, report:

1. files changed;
2. current behavior affected;
3. target architecture affected;
4. HTDS/ADR/RFC references;
5. tests run;
6. tests not run;
7. known risks;
8. compatibility impact;
9. Linux/Proton impact;
10. follow-up work.

Do not hide failures.

---

## 23. Pull Request Checklist for Agents

Before completing work, verify:

- [ ] Relevant HTDS documents were read.
- [ ] Current code was inspected.
- [ ] No API or implementation was fabricated.
- [ ] The change is incremental.
- [ ] Existing behavior is preserved or documented.
- [ ] Context implications were considered.
- [ ] Authority implications were considered.
- [ ] Protocol and persistence implications were considered.
- [ ] Linux and Proton implications were considered.
- [ ] Tests or validation were performed where practical.
- [ ] Untested behavior is disclosed.
- [ ] Documentation status remains truthful.
- [ ] External licenses were reviewed when applicable.
- [ ] The final report lists risks and follow-ups.

---

## 24. Final Directive

Be ambitious about Halcyon's destination and conservative about unvalidated changes.

Preserve what works.

Document what is planned.

Prototype the smallest useful slice.

Validate it honestly.

Then generalize.
