# HTDS-002 — Vision

| Field | Value |
| --- | --- |
| Document ID | HTDS-002 |
| Title | Vision |
| Version | 0.1.0 |
| Status | Draft |
| Audience | All Contributors |

# 1. Vision Statement

Halcyon aims to transform Skyrim from a synchronized single-player experience into a true multiplayer platform.

Rather than focusing on a single style of gameplay, Halcyon provides a reusable multiplayer foundation capable of supporting multiple experiences without requiring architectural changes to the engine.

The platform is intended to support both traditional cooperative gameplay and entirely new multiplayer experiences built on top of Skyrim.

# 2. Core Philosophy

## Shared World

Players exist in the same physical world and should be able to see, communicate, cooperate, fight, trade and explore together whenever possible.

## Independent Narratives

Narrative progression belongs to Contexts rather than the world itself, allowing incompatible quest states to coexist safely.

## Server Authority

Clients request actions. The server validates, decides and replicates the resulting state.

# 3. Project Identity

Halcyon is not an MMORPG.
Halcyon is not merely a Skyrim Together fork.
Halcyon is a multiplayer platform.

# 4. Long-Term Vision

The architecture should naturally support runtime quests, dynamic events, bounty hunts, guilds, player housing, territory control, seasonal events, world bosses and persistent economies.

# 5. Context-Oriented Multiplayer

Contexts define the logical scope in which state exists.

Examples:

- Global
- Personal
- Party
- Narrative Instance
- Runtime Event
- Dungeon Instance
- Scripted Scenario

Visibility is determined by Context membership rather than geographic distance alone.

# 6. Vanilla Compatibility

Compatibility with Skyrim remains a primary objective.

# 7. Linux Commitment

Linux is a first-class development and deployment platform.

# 8. Extensibility

Gameplay should increasingly move into official extension APIs rather than engine modifications.

# 9. Community

Subsystem boundaries should allow contributors to work independently.

# 10. Success Criteria

- Independent campaigns coexist.
- Persistent public servers.
- Runtime systems without engine modifications.
- Linux first-class support.
- Progressive server authority.

# 11. Final Vision

Halcyon provides Skyrim with a modern multiplayer architecture while preserving the strengths of Skyrim Together Reborn.

> One World. Many Stories.
