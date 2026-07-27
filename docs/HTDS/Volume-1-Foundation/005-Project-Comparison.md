# HTDS-005 — Project Comparison

| Field | Value |
| --- | --- |
| Document ID | HTDS-005 |
| Title | Skyrim Together, SkyMP and Halcyon |
| Version | 0.1.0 |
| Status | Draft |

## Purpose

This document explains why Halcyon evolves the Tilted Evolution codebase while borrowing architectural concepts from SkyMP instead of replacing its client stack.

## High-Level Comparison

| Capability | Skyrim Together | SkyMP | Halcyon |
| --- | --- | --- | --- |
| Vanilla co-op | Excellent | Limited | Primary objective |
| Public persistent servers | Limited | Strong | Primary objective |
| Native Linux direction | Existing work | Limited | First-class |
| Runtime quests | Minimal | Supported | Core feature |
| Context-based narrative | No | Partial concepts | Core architecture |
| Server authority | Partial | Stronger | Long-term target |
| Plugin/game modes | Limited | Strong | Planned |

## Skyrim Together Strengths

- Mature vanilla synchronization.
- Existing networking foundation.
- Proven gameplay compatibility.
- Native client integration.
- Existing Linux/Proton work in this fork.

## Skyrim Together Limitations

- Client-authoritative assumptions.
- Shared quest progression.
- Limited support for persistent public worlds.
- Difficult to extend with server-driven gameplay.

## SkyMP Strengths

- Server-side world representation.
- Runtime scripting.
- Extensible game modes.
- Better separation between engine and gameplay.
- Persistent multiplayer concepts.

## SkyMP Limitations

- Different design priorities.
- Client stack centered around Skyrim Platform.
- Less focused on vanilla cooperative compatibility.
- Higher adoption cost for the existing Linux fork.

## Halcyon Strategy

Halcyon intentionally combines the strongest aspects of both projects.

From Tilted Evolution:

- networking;
- vanilla synchronization;
- native client;
- Linux work.

Inspired by SkyMP:

- authoritative server;
- world state;
- runtime quests;
- game modes;
- persistence;
- plugin architecture.

## Guiding Decision

Halcyon evolves the existing client while redesigning the server.

The project avoids unnecessary rewrites and preserves proven functionality whenever possible.

## Closing Statement

Halcyon is not intended to compete with Skyrim Together or SkyMP.

It builds upon their achievements while pursuing a different objective: a context-oriented multiplayer platform capable of supporting both vanilla cooperative gameplay and persistent public worlds.
