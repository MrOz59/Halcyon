# HTDS-004 — Non-Goals

| Field | Value |
| --- | --- |
| Document ID | HTDS-004 |
| Title | Non-Goals |
| Version | 0.1.0 |
| Status | Draft |

## 1. Purpose

This document explicitly defines what Halcyon is **not** trying to achieve.

Clearly identifying non-goals prevents architectural drift and helps contributors evaluate proposals consistently.

## 2. Halcyon is NOT...

### 2.1 A replacement for Skyrim

Halcyon extends Skyrim's multiplayer capabilities. It does not attempt to replace the Creation Engine or recreate Skyrim from scratch.

### 2.2 A complete MMORPG

Persistent worlds and MMO-like features are supported, but Halcyon does not aim to become a traditional MMORPG with mandatory progression systems.

### 2.3 A replacement for the Creation Kit

Quest authoring, world editing, assets, and plugin creation remain the responsibility of Bethesda's existing toolchain.

### 2.4 A total rewrite of Skyrim Together

The project intentionally evolves the Tilted Evolution foundation instead of discarding years of engineering work.

### 2.5 A Windows-only project

Linux and Proton are first-class targets.

### 2.6 An engine for arbitrary games

The architecture is optimized for Skyrim and closely related Bethesda technologies.

### 2.7 An anti-cheat product

Security is important, but Halcyon's goal is authoritative multiplayer, not kernel-level anti-cheat.

### 2.8 A framework that guarantees every Skyrim mod will work online

Single-player mods were not designed with multiplayer in mind. Compatibility must be evaluated individually.

## 3. Deferred Goals

The following may be implemented in the future but are intentionally out of scope for the initial releases:

- clustered servers;
- cross-server travel;
- distributed simulation;
- official matchmaking;
- cloud persistence services;
- mobile companion applications.

## 4. Guiding Principle

When a proposal increases complexity without advancing the goals defined in HTDS-003, it SHOULD be rejected or deferred.

Keeping the core focused is considered essential to the long-term maintainability of Halcyon.

## 5. Closing Statement

Halcyon succeeds by doing a limited number of difficult things well.

Every feature added to the project should reinforce its identity as a server-authoritative, context-oriented multiplayer platform rather than expanding its scope indiscriminately.
