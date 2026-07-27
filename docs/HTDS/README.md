# Halcyon Technical Design Specification

**Status:** Draft  
**Current specification version:** 0.1.0  
**Language:** English

The Halcyon Technical Design Specification (HTDS) is the authoritative description of the intended architecture, design principles, and long-term technical direction of the Halcyon multiplayer platform.

The source code describes how the current implementation behaves. The HTDS describes how Halcyon is intended to behave and why major architectural decisions were made. When the implementation and specification disagree, the discrepancy must be documented and resolved deliberately.

## Volumes

| Volume | Subject | Status |
| --- | --- | --- |
| [Volume 1 — Foundation](Volume-1-Foundation/README.md) | Vision, goals, terminology, comparisons, and design principles | In progress |
| Volume 2 — Core Architecture | Server, client, networking, world state, entities, replication, and persistence | Planned |
| Volume 3 — Gameplay Systems | Contexts, narrative instances, vanilla quests, runtime quests, PvP, events, and bounties | Planned |
| Volume 4 — SDK and Extensibility | C++, TypeScript, Papyrus integration, plugins, and mod compatibility | Planned |
| Volume 5 — Infrastructure | Deployment, databases, operations, security, observability, and administration | Planned |
| Appendices | Protocol layouts, diagrams, examples, migration notes, and compatibility matrices | Planned |

## Document identifiers

HTDS documents use stable numeric identifiers:

- `HTDS-000` through `HTDS-099`: foundation and project-wide material;
- `HTDS-100` through `HTDS-199`: core architecture;
- `HTDS-200` through `HTDS-299`: gameplay systems;
- `HTDS-300` through `HTDS-399`: SDK and extensibility;
- `HTDS-400` through `HTDS-499`: infrastructure and operations;
- `HTDS-900` through `HTDS-999`: appendices and reference material.

The identifier remains stable even when a file is renamed.

## Normative language

The terms **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are normative when written in uppercase.

## Supporting documents

- [`docs/ADR`](../ADR/README.md) records accepted architectural decisions.
- [`docs/RFC`](../RFC/README.md) contains proposals that are not yet accepted architecture.
- [`docs/Research`](../Research/README.md) contains technical investigations and external-project analysis.

## Contribution rule

Substantial new subsystems SHOULD have an HTDS section or RFC before implementation. Changes that intentionally diverge from the current HTDS MUST update the relevant document or create an ADR explaining the divergence.
