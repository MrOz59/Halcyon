# Halcyon

> A next-generation multiplayer platform for Skyrim, built by evolving the Linux fork of Tilted Evolution rather than replacing it.

> **Project status:** Early research and architecture phase.

## Vision

Halcyon aims to transform the existing Skyrim Together Linux port into a platform capable of supporting:

- Persistent public servers
- Progressive server authority
- Context-aware world state
- Independent quest progression
- Runtime server events
- Runtime quests
- Plugin and game mode support
- First-class Linux and Proton support

The project is intentionally evolutionary rather than revolutionary.

## Why Halcyon?

Existing multiplayer implementations work well for small cooperative sessions, but public persistent worlds expose limitations such as shared quest state, client authority and limited extensibility.

Halcyon introduces new architectural concepts while preserving as much of the existing codebase as practical.

## Current Status

The repository currently contains:

- Linux port of Tilted Evolution (foundation)
- HTDS (Halcyon Technical Design Specification)
- ADRs
- RFCs
- Research documents
- Early architectural planning

Many systems described in the HTDS are **design targets**, not implemented features.

## Project Principles

- Incremental migration instead of complete rewrites
- Documentation-driven architecture
- Progressive server authority
- Contexts as the primary state-isolation mechanism
- Honest implementation status
- AI-assisted development with mandatory human validation

## Documentation

| Directory | Purpose |
|-----------|---------|
| docs/HTDS | Technical Design Specification |
| docs/ADR | Accepted architectural decisions |
| docs/RFC | Proposed designs |
| docs/Research | Reverse engineering, experiments and comparisons |

Please read **CONTRIBUTING.md** and **AGENTS.md** before contributing.

## Roadmap

### Phase 1
- Documentation
- Repository organization
- Architecture definition
- Source-code mapping

### Phase 2
- Context prototype
- Context-aware replication
- World-state prototype
- Persistence prototype

### Phase 3
- Runtime Quest system
- Plugin Host
- Public events
- Server-authoritative gameplay

### Phase 4
- Production-ready architecture
- Stable SDK
- Large-scale public servers

## Contributing

Contributions are welcome.

Human contributors remain responsible for reviewing and validating all AI-generated content.

Please read:

- CONTRIBUTING.md
- AGENTS.md

before submitting changes.

## License

See the repository license.
