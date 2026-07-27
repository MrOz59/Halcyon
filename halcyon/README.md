# Halcyon Modules

This directory is reserved for new Halcyon-specific modules that are not yet appropriate to place directly into the existing Tilted Evolution layout.

The legacy source tree will not be moved merely to create a cleaner directory structure. Migration must be incremental and supported by the build system.

Planned areas:

- `server/` — authoritative world state, Contexts, replication, persistence, runtime quests, and plugins;
- `client/` — Context-aware adapters, runtime-quest UI, and integrations;
- `sdk/` — public C++ and scripting interfaces;
- `tools/` — data inspection, migration, validation, and administration.

Directories will be introduced with real modules rather than empty placeholders.
