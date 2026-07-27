# ADR-0001 — Evolve Tilted Evolution Instead of Adopting the SkyMP Client

**Status:** Accepted  
**Date:** 2026-07-27

## Context

Halcyon requires both strong compatibility with vanilla Skyrim cooperative gameplay and a more authoritative, extensible server architecture.

Tilted Evolution already provides deep game integration, synchronization, a working protocol, and the Linux/Proton client work maintained by this project.

SkyMP provides valuable server architecture concepts but its client is built around Skyrim Platform and Chromium, increasing complexity and Linux/Proton risk.

## Decision

Halcyon will retain and evolve the Tilted Evolution client and protocol foundation.

The server will be redesigned incrementally using server-authoritative concepts studied from SkyMP and other multiplayer architectures.

Halcyon will not initially adopt the SkyMP client, Skyrim Platform, or Chromium.

## Consequences

Positive:

- Existing Linux work is preserved.
- Vanilla cooperative behavior remains the starting point.
- TrueHUD and native ImGui integrations remain usable.
- Migration can be incremental.

Negative:

- The current server architecture requires substantial redesign.
- Legacy client-authoritative paths will coexist temporarily with authoritative systems.
- Some SkyMP components may require adaptation rather than direct reuse.

## Alternatives considered

1. Replace Tilted Evolution completely with SkyMP.
2. Maintain only the Linux compatibility fork without changing server architecture.
3. Build a completely new Skyrim multiplayer implementation.

All three were rejected as either discarding too much existing work or failing to meet the public-server goals.
