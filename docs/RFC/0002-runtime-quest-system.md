# RFC-0002 — Runtime Quest System

**Status:** Draft  
**Authors:** Halcyon Project Contributors

## Summary

This RFC proposes a server-owned Runtime Quest system for activities that are not required to exist as native Skyrim `TESQuest` records.

## Goals

The system should support:

- public events;
- bounty hunts;
- escorts;
- world bosses;
- defense events;
- treasure hunts;
- server contracts.

## Initial Model

```cpp
enum class RuntimeQuestStatus
{
    Draft,
    Active,
    Completed,
    Failed,
    Expired,
    Cancelled
};

struct RuntimeQuest
{
    RuntimeQuestId id;
    ContextId context;
    RuntimeQuestStatus status;
    std::string title;
    std::vector<RuntimeObjective> objectives;
    std::vector<RuntimeReward> rewards;
    TimePoint expiresAt;
};
```

## Authority

The server owns:

- creation;
- enrollment;
- objective progress;
- completion;
- expiry;
- contribution;
- rewards.

The client only:

- displays quest data;
- submits Intents and Observations;
- renders markers and notifications.

## Initial Prototype

The first prototype should implement one public world-boss event:

1. server creates event;
2. eligible Players receive event metadata;
3. server spawns or selects one Actor;
4. accepted damage contributes progress;
5. server validates Actor death;
6. event completes;
7. eligible participants receive one persisted reward;
8. reconnect does not duplicate the reward.

## UI

The first version may use the native ImGui overlay.

Native Skyrim journal integration is out of scope.

## Persistence

Persist:

- Runtime Quest ID;
- Context;
- status;
- objective state;
- participants;
- reward grants;
- expiry.

## Security

Clients must not declare:

- completion;
- reward amount;
- participant eligibility;
- contribution;
- expiry.

## Out of Scope

- procedural dialogue;
- Creation Kit authoring;
- native journal injection;
- complex branching;
- dynamic Papyrus generation;
- cross-server events.

## Acceptance Criteria

- event survives server restart;
- completion is server-owned;
- reward is granted once;
- unrelated Players cannot claim the reward;
- unsupported clients fail safely.

## Open Questions

- C++ templates or script-defined templates?
- How should contribution be measured?
- Can events reuse vanilla Actors safely?
- How are map markers implemented?
- How are rewards mapped to local inventory securely?
