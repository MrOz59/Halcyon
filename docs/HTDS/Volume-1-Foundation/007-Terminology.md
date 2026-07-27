# HTDS-007 — Terminology

| Field | Value |
| --- | --- |
| Document ID | HTDS-007 |
| Title | Terminology |
| Version | 0.1.0 |
| Status | Draft |
| Maturity | Conceptual / Pre-Implementation |
| Audience | All contributors |

## 1. Purpose

This document defines the vocabulary used throughout the Halcyon Technical Design Specification.

The terms below describe the target architecture. Some do not yet correspond to concrete classes, packets, database tables, or public APIs.

Unless another HTDS document explicitly defines a narrower meaning, contributors SHOULD use the terminology in this document consistently in:

- source code;
- code review;
- issue discussions;
- RFCs;
- ADRs;
- protocol documentation;
- user-facing technical documentation.

Terms marked as **conceptual** are architectural ideas whose final implementation remains open.

## 2. General Terms

### Halcyon

The multiplayer platform described by the HTDS and developed from the Linux/Proton fork of Tilted Evolution.

Halcyon includes the client, server, protocol, persistence systems, extension APIs, documentation, and supporting tools.

### Tilted Evolution

The open-source codebase behind Skyrim Together Reborn and the technical foundation inherited by Halcyon.

### Skyrim Together Reborn

The multiplayer mod and user-facing project built from Tilted Evolution.

Within technical documents, **STR** may be used as an abbreviation when the meaning is unambiguous.

### SkyMP

A separate open-source Skyrim multiplayer project studied by Halcyon for server-side world representation, game modes, plugin-data parsing, scripting, persistence, and related architecture.

SkyMP is not the Halcyon client foundation.

### Client

The Halcyon component running inside or alongside a player's Skyrim process.

The client is responsible for:

- integrating with the game runtime;
- collecting local input and observations;
- rendering remote entities;
- applying accepted server state;
- presenting multiplayer UI;
- sending requests and reports to the server.

A client is not assumed to be authoritative merely because it executes the Skyrim engine.

### Server

The dedicated Halcyon process that coordinates sessions and is intended to become the authoritative owner of multiplayer state.

### Session

A live authenticated connection between one client and one server.

A player may reconnect through a new Session while retaining the same persistent identity.

### Player

A persistent user identity represented on the server.

A Player is distinct from:

- a live Session;
- the local Skyrim actor;
- a remote actor representation;
- a character profile, if multiple characters are supported later.

### Character

A gameplay identity associated with a Player.

The initial implementation may treat Player and Character as equivalent, but future persistence systems may allow one Player to own multiple Characters.

### Actor

A Skyrim runtime entity capable of actor behavior, such as a player character, NPC, creature, or summon.

### Entity

A server-recognized object with an identity and replicated or persistent state.

An Entity may represent:

- a player;
- an actor;
- an object reference;
- a projectile;
- a runtime event object;
- a server-defined logical object.

The final Entity model is not yet specified.

### Form

A record defined by Skyrim or a loaded plugin and identified through a FormID.

### FormID

The identifier used by Skyrim to reference plugin-defined records and runtime references.

A FormID is not automatically stable across arbitrary load orders unless it is normalized against plugin identity.

### Reference

A placed or runtime-instantiated object in the game world.

A Reference usually points to a base Form and has state such as position, enabled state, inventory, ownership, or scripts.

### Base Form

The underlying form that defines the type and default data of a Reference.

### Cell

A Skyrim world partition used to group references and loaded simulation state.

### Worldspace

A large exterior world composed of cells.

## 3. Authority Terms

### Authority

The right to decide the accepted value or outcome of a piece of multiplayer state.

Authority may belong to:

- the server;
- a client;
- a Context;
- a subsystem;
- a temporary owner;
- a script or game mode.

### Server Authority

A model in which the server determines the accepted state and clients apply the result.

### Client Authority

A model in which a client determines state and the server primarily forwards or records it.

Client authority may remain temporarily in legacy systems.

### Progressive Authority

The migration strategy in which individual systems move from client-authoritative behavior toward server validation and server authority over time.

### Authority Mode

A classification describing how a subsystem or state field is controlled.

Conceptual values include:

- `LegacyClient`;
- `ClientOwnedServerRecorded`;
- `ServerValidated`;
- `ServerAuthoritative`;
- `Cosmetic`.

These names are not yet API commitments.

### Owner

The identity currently permitted to produce or modify a state stream.

Ownership is narrower than authority.

For example, the server may remain authoritative while assigning one client responsibility for reporting movement samples.

### Validation

The process of determining whether a requested action or reported state is legal, plausible, authorized, and consistent with current server state.

### Reconciliation

The process of resolving differences between client state and authoritative server state.

### Revision

A monotonically increasing or otherwise ordered identifier used to distinguish newer authoritative state from stale state.

### Stale State

State based on an older revision, snapshot, save, Session, or world view.

### Intent

A client request describing what the player attempted to do rather than declaring the final outcome.

Examples:

- activate a door;
- attack an actor;
- accept a bounty;
- interact with a quest actor.

### Observation

A client report describing something the local Skyrim runtime observed.

Observations may require validation and are not necessarily authoritative.

### Outcome

The accepted result of an Intent or validated event.

## 4. Context Terms

### Context

A logical scope that owns, selects, or filters state.

A Context may define:

- participants;
- quest state;
- entity state;
- object changes;
- replication visibility;
- persistence behavior;
- lifecycle rules;
- branch decisions.

The final Context data model remains conceptual.

### Context ID

A server-issued identifier for one Context instance.

### Context Type

A category describing the purpose of a Context.

Possible conceptual types include:

- Global;
- Personal;
- Party;
- Narrative;
- Dungeon;
- Runtime Quest;
- Public Event;
- Server Defined.

### Context Membership

The relationship indicating that a Player, Session, Entity, or subsystem participates in or can observe a Context.

### Active Context

A Context currently influencing a Player's effective state.

A Player may eventually have several active Contexts simultaneously.

### Global Context

The shared server scope used for state intended to be common across the public world.

Possible examples include:

- connected players;
- global server rules;
- public weather;
- public events;
- global PvP settings.

Not all physical world state must be global.

### Personal Context

A Context associated with one Player or Character.

It may contain:

- solo quest progression;
- personal decisions;
- personal quest actors;
- personal scripted references.

### Party Context

A Context shared by members of a Party.

It may contain:

- cooperative quest progression;
- shared dungeon state;
- shared objectives;
- party-specific actors and objects.

### Narrative Context

A Context created to represent a particular story state, branch, or quest progression.

A Personal or Party Context may also be a Narrative Context.

### Runtime Quest Context

A Context created for one server-generated Runtime Quest.

### Public Event Context

A Context used by an event that may accept many participants without requiring them to join one Party.

### Dungeon Context

A Context controlling an instanced or phased dungeon state.

### Context Inheritance

A possible future mechanism through which one Context derives default state from another.

Context inheritance is not yet accepted architecture.

### Context Composition

The process of combining state selected from multiple active Contexts into one effective client-visible world view.

### Context Conflict

A situation in which two active Contexts define incompatible state for the same quest, Entity, Reference, or branch.

### Context Resolution

The policy used to choose, isolate, merge, or reject conflicting Context state.

### Context Transition

A Player joining, leaving, activating, deactivating, or switching Contexts.

Examples include:

- joining a Party;
- entering an instanced dungeon;
- accepting a Runtime Quest;
- leaving a narrative instance.

### Context Isolation

The prevention of state changes from one Context affecting unrelated Contexts.

### Contextual Relevance

The rule determining whether state from a Context should be visible or replicated to a recipient.

## 5. World-State Terms

### World State

The server's representation of multiplayer-relevant state.

The target World State may include:

- Entities;
- References;
- actor values;
- ownership;
- Context state;
- quest state;
- runtime events;
- persistent changes.

It does not need to reimplement every Creation Engine subsystem.

### Base World

The default state derived from Skyrim and the configured plugin set before Halcyon-specific changes are applied.

### Effective World View

The world state produced for one recipient after combining base data, authoritative state, active Contexts, and replication policy.

### Change Form

A representation of changes relative to base game or plugin data.

The term is inspired by Skyrim save data and SkyMP architecture, but Halcyon's final format may differ.

### Scoped Change Form

A Change Form associated with a specific Context.

### Delta

A change from a previous or base state rather than a complete replacement snapshot.

### Snapshot

A complete representation of relevant state at a particular revision or time.

### Persistent State

State that survives Session loss or server restart.

### Ephemeral State

State that may be discarded when a Session, encounter, or process ends.

### Durable State

Persistent state considered successfully committed to storage.

### Tombstone

A persistent marker indicating that an Entity or record was deleted, removed, or invalidated.

### World Mutation

An accepted change to World State.

Examples include:

- actor death;
- door unlock;
- container update;
- Reference enable or disable;
- quest-stage change.

## 6. Replication Terms

### Replication

The transmission and application of state changes between server and clients.

### Replication Scope

The set of recipients eligible to receive a state update.

### Spatial Relevance

Relevance determined by world position, cell, worldspace, distance, or loaded area.

### Contextual Relevance

Relevance determined by Context membership or compatibility.

### Permission Relevance

Relevance determined by access rules, moderation, ownership, party membership, or game-mode policy.

### Interest Management

The broader system that determines which state each client should receive.

### Recipient

A Session selected to receive a replicated message or state update.

### Broadcast

Replication to several or all eligible recipients.

A broadcast does not necessarily mean every connected client.

### Unicast

A message sent to one recipient.

### Multicast

A message sent to a selected group of recipients.

### Replication Filter

A rule or pipeline stage that accepts or rejects a potential recipient.

### Replication Priority

The relative importance of sending one update before another when bandwidth or processing is constrained.

### Full State

A complete state representation sent for initialization or recovery.

### Delta Update

An update containing only changes since a known state or revision.

### Resynchronization

The process of restoring a client to a correct authoritative state after divergence or missing updates.

### Divergence

A condition in which two participants hold incompatible state that should be consistent.

### Desynchronization

User-visible or system-visible divergence between clients or between a client and server.

## 7. Narrative and Quest Terms

### Quest State

The multiplayer-relevant state associated with a quest.

It may include:

- current stages;
- displayed objectives;
- completed objectives;
- branch decisions;
- aliases;
- related actor state;
- related object state.

### Quest Stage

A numeric stage used by Skyrim quests.

A larger stage number MUST NOT be assumed to represent universally newer or preferable progression.

### Quest Objective

A user-visible or logical task associated with a quest.

### Quest Branch

A mutually exclusive or meaningfully divergent narrative path.

### Branch Decision

The state indicating which Quest Branch was selected.

### Quest Alias

A Skyrim quest mechanism that binds actors, references, locations, or data to quest roles.

### Quest Fragment

Compiled Papyrus logic associated with quest stages, dialogue, scenes, or other quest events.

### Vanilla Quest

A quest authored in Skyrim or installed plugin data and executed by the Skyrim quest system.

### Runtime Quest

A multiplayer activity created and owned by the Halcyon server at runtime.

A Runtime Quest does not need to be a native `TESQuest`.

### Quest Adapter

The planned client subsystem that reads or applies supported Vanilla Quest state and presents server-driven quest behavior to the local game.

### Narrative Instance

A temporary or persistent isolated state used when incompatible story outcomes cannot safely coexist in one shared effective world view.

### Phasing

Showing different narrative Entities or Reference states to different Players while they occupy the same general location.

### Cell Instance

A separately managed version of a Cell or group of Cells for a specific Context.

### Guest Progression

A possible mode in which a Player participates in another Context's quest without immediately committing all progress to personal state.

Guest Progression is conceptual and requires a dedicated RFC.

### Progress Commit

The process of applying compatible Party or instance progress to persistent Personal state.

### Progress Merge

The process of combining quest state from different Contexts.

Automatic merge is not assumed to be safe.

### Fast-Forward

Advancing one quest state to a later compatible state without replaying all intermediate events.

### Rollback

Returning authoritative state to an earlier revision.

Quest rollback is expected to be unsafe in many cases and should not be treated as a general solution.

## 8. Runtime-Content Terms

### Runtime Content

Gameplay state or activities created by the server rather than authored entirely as fixed Vanilla Quest content.

### Runtime Quest Manager

The planned server subsystem responsible for creating, tracking, completing, expiring, and persisting Runtime Quests.

### Runtime Objective

One server-owned task within a Runtime Quest.

### Public Event

A server-created activity available to eligible Players in a region or across the server.

### World Boss

A server-managed high-value encounter intended for several Players or Parties.

### Bounty

A server-managed reward associated with capturing, defeating, or otherwise resolving a target.

### Bounty Target

The Player, Actor, or Entity identified by a Bounty.

### Hunter

A Player or Party registered to pursue a Bounty.

### Claim

A request to receive the reward for a completed bounty or event.

Claims must be server-validated.

### Reward

A server-approved benefit granted after a validated outcome.

Examples include:

- gold;
- items;
- reputation;
- server currency;
- titles;
- progression.

### Event Participant

A Player eligible for progress, credit, or rewards in a Public Event.

### Contribution

A server-measured action used to determine participation or reward eligibility.

## 9. Party and Social Terms

### Party

A server-managed group of Players who explicitly choose to cooperate.

### Party Leader

The Player with specific administrative permissions within a Party.

Party leadership does not automatically grant authority over every member's persistent narrative state.

### Party Membership

The server-owned relationship connecting a Player to a Party.

### Party Progression

Quest or Runtime Quest state shared through a Party Context.

### Solo Player

A Player not currently participating in a Party Context for the relevant activity.

### Public Player

A Player visible or interactive in the shared world without being a Party member.

## 10. Persistence Terms

### Persistence Layer

The subsystem responsible for durable storage and recovery.

### Persistence Adapter

An implementation connecting Halcyon persistence interfaces to a database or storage backend.

### Transaction

A set of persistence changes committed atomically or treated as one logical operation.

### Checkpoint

A durable state marker used to simplify recovery.

### Journal

An ordered record of changes used for audit or replay.

This term should be qualified as **persistence journal** to avoid confusion with Skyrim's quest journal.

### Event Log

An ordered record of accepted domain events.

### Recovery

Reconstructing valid state after a process failure, storage failure, disconnect, or restart.

### Idempotency

The property that repeating one operation does not apply its effects more than once.

### Migration

A controlled transformation of code, protocol, configuration, or stored data from one version to another.

### Import

The one-time or limited ingestion of external state, such as a local save or legacy server database.

Import does not grant continuing authority to the external source.

## 11. Protocol Terms

### Protocol

The versioned set of messages, serialization rules, capabilities, and interaction flows used between client and server.

### Protocol Version

The compatibility identifier for the network contract.

Protocol Version is distinct from the source commit or build identity.

### Build Identity

The commit, version, or package identifier used for diagnostics and release tracking.

### Capability

A named feature that one endpoint supports.

Capabilities may allow compatible clients and servers to negotiate optional behavior.

### Message

One serialized protocol unit.

### Packet

A transport-level unit that may contain one or more messages or part of a message.

### Request

A message asking another component to perform or validate an operation.

### Notification

A message announcing an accepted event or state change without requiring a direct response.

### Command

A typed instruction from one trusted component to another.

Server-to-client commands must be constrained and versioned.

### Acknowledgement

A message confirming receipt, acceptance, or durable processing.

### Correlation ID

An identifier linking requests, responses, logs, and asynchronous operations.

### Backward Compatibility

The ability of a newer component to interoperate safely with an older contract.

### Forward Compatibility

The ability of an older component to ignore or safely handle newer optional information.

## 12. Game-Data Terms

### Game Data Service

The planned server subsystem that loads and exposes multiplayer-relevant metadata from Skyrim plugins.

### Plugin

A Skyrim data file such as ESM, ESP, or ESL.

Within SDK documents, **Halcyon Plugin** should be used for a server extension to avoid ambiguity.

### Plugin Set

The ordered collection of Skyrim plugins configured for one server.

### Load Order

The order in which Skyrim plugins are loaded and FormIDs are resolved.

### Mod Manifest

A server-provided description of required content, versions, hashes, and load-order expectations.

### Normalized Form Identity

A Form reference represented using plugin identity and local record identity rather than only the runtime load-order-dependent FormID.

### Record

One structured entry in a Skyrim plugin.

### Parser

A component that reads plugin or save-file data.

### Native Function

A Papyrus function implemented by the game engine or another host rather than Papyrus bytecode.

## 13. Extension Terms

### Halcyon Plugin

A server or client extension using an official Halcyon extension API.

### Game Mode

A high-level package of server gameplay rules and systems.

### Plugin Host

The subsystem responsible for loading, isolating, invoking, and unloading Halcyon Plugins or Game Modes.

### Extension API

A versioned public interface provided to external modules.

### SDK

The headers, schemas, libraries, tools, examples, and documentation needed to build Halcyon extensions.

### Hook

A callback or interception point tied to internal or engine behavior.

Hooks are not automatically stable public APIs.

### Event

A structured notification that something occurred.

### Domain Event

An accepted gameplay or system event with defined meaning inside Halcyon's architecture.

### Script Runtime

A managed environment used to execute high-level Game Mode or plugin logic.

TypeScript is under consideration but not yet selected.

### Sandbox

A boundary limiting what scripts or plugins can access or damage.

## 14. Client-Integration Terms

### Native UI

A client interface implemented through native code and rendering integration rather than an embedded browser runtime.

### Overlay

A user interface rendered over the Skyrim frame.

### Quest Adapter

The planned client bridge between server quest state and supported local Skyrim quest operations.

### Context Adapter

The planned client subsystem applying Context-specific state to the local Skyrim view.

### Remote Entity

A local representation of an Entity primarily controlled by server or another client.

### Local Player Actor

The Skyrim actor controlled directly by the user on one client.

### Client Command

A typed server instruction requesting one approved local engine operation.

### TrueHUD Integration

An optional client integration using TrueHUD APIs to display remote-player or multiplayer-specific HUD elements.

### Proton Path

The launcher and runtime behavior used when the Windows Skyrim executable runs under Proton or Wine.

## 15. Operational Terms

### Server Operator

A person or organization responsible for configuring and running one Halcyon server.

### Administrator

A trusted identity with management permissions on one server.

### Moderator

A trusted identity with limited enforcement or community-management permissions.

### Audit Log

A durable record of administrative, security-sensitive, or economy-sensitive actions.

### Metric

A numeric operational measurement collected over time.

### Trace

A correlated record of one request or event flow across subsystems.

### Health Check

An endpoint or test indicating whether a service can perform its essential responsibilities.

### Graceful Shutdown

A controlled shutdown that stops accepting new work and persists required state before exiting.

## 16. Status Terms

### Conceptual

The architecture is described but has not been implemented.

### Prototype

An experimental implementation exists to validate assumptions.

### Partially Implemented

Some intended behavior exists, but major requirements remain incomplete.

### Implemented

The intended behavior is substantially represented in production code.

### Validated

Implementation behavior has been tested against documented acceptance criteria.

### Legacy

Existing behavior inherited from Tilted Evolution that has not yet migrated to the target architecture.

### Transitional

Temporary architecture intentionally bridging Legacy and target behavior.

### Deprecated

Still present but discouraged and scheduled for removal or replacement.

### Superseded

Replaced by a newer document, API, subsystem, or decision.

## 17. Naming Guidance

The following naming distinctions SHOULD be preserved:

- Use **Player** for persistent user identity.
- Use **Session** for one live connection.
- Use **Actor** for a Skyrim actor.
- Use **Entity** for a server-recognized replicated object.
- Use **Context** for state scope.
- Use **Instance** only when state is actually isolated.
- Use **Runtime Quest** for server-created activities.
- Use **Vanilla Quest** for Skyrim quest-system content.
- Use **Plugin** with qualification when ambiguity exists.
- Use **Protocol Version** separately from **Build Identity**.
- Use **Owner** separately from **Authority**.
- Use **Observation** separately from **Outcome**.

## 18. Open Terminology Questions

The following terms require refinement in future RFCs:

- whether **Context** and **Instance** need separate persistent identifiers;
- whether **Narrative Context** is a Context Type or an attribute;
- whether **Character** should become distinct from Player in the first persistence model;
- whether **Entity** should include purely logical server objects;
- whether Halcyon should use **Change Form**, **State Delta**, or another term for persistent scoped mutations;
- whether **Game Mode** and **Halcyon Plugin** should remain separate extension categories;
- whether **Runtime Quest** should include Public Events or whether both should share a broader parent term.

Until resolved, contributors should use the definitions in this document and clearly label alternative terminology as proposed.

## 19. Closing Statement

Consistent language is a prerequisite for consistent architecture.

This glossary is expected to evolve as prototypes become real systems, but terminology should change through deliberate HTDS updates or ADRs rather than through accidental variation in code.
