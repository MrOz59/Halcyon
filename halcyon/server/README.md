# Halcyon Server Architecture

This area will host new server-authoritative modules as they are introduced.

Planned boundaries:

- **world** — server-owned entities and world state;
- **contexts** — global, personal, party, event, dungeon, and narrative scopes;
- **replication** — spatial and narrative relevance;
- **persistence** — Context-scoped change forms and recovery;
- **runtime-quests** — server-created objectives and events;
- **plugins** — stable extension boundaries and game-mode hosting.

The current `Code/server` implementation remains authoritative for the working build until individual modules are migrated deliberately.
