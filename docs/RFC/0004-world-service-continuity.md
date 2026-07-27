# RFC-0004 — World Service Continuity

**Status:** Draft
**Authors:** Halcyon Project Contributors

## Summary

This RFC proposes treating the death of world-critical NPCs as a **service
continuity** problem rather than a narrative isolation problem.

It argues that RFC-0001's framing — scope an Actor's life state per Context —
is both too broad and too narrow for the case that actually breaks public
servers, and proposes classifying NPCs so that different categories get
different treatment.

## Problem

On a persistent public server, one player killing an NPC can permanently remove
a service every other player depends on.

The categories are not equivalent:

| NPC | Death matters because | Global death acceptable? |
| --- | --- | --- |
| Generic (bandit, wolf, draugr) | It does not | Yes |
| Merchant or other service provider | The world stops offering that service to everyone | No |
| Owner of a non-essential quest | It blocks that quest for players who have not done it | No, but only for those players |

RFC-0001 treats all three identically. That is wrong in both directions:

- it scopes generic deaths, which nobody needs scoped, at the cost of fighting
  the entire synchronization pipeline;
- it does not solve the merchant case at all. "A sees dead, B sees alive" still
  leaves the merchant permanently dead **for A**, so A's economy is broken just
  as thoroughly as before, only privately.

The observation driving this RFC is that a merchant's death is not a story
branch. It is an outage.

## Evidence in the current code

Skyrim already distinguishes these categories, and the server already reads
that data.

`Code/components/es_loader/Records/Chunks.h` parses the `ACBS` chunk of every
`NPC_` record, which carries:

```text
kEssential   — cannot be killed; the game already protects these
kProtected   — only the player may kill it
kUnique      — not an interchangeable generic
kRespawn     — the game itself reconstitutes this actor
```

`Chunks::ACBS` already provides `IsEssential()`, `IsProtected()`, `IsUnique()`
and `IsRespawn()` accessors, and `RecordCollection::GetNpcById` exposes the
parsed record to the server.

**No code currently calls any of them.** The flags are parsed and discarded.

This matters because it means classification does not require a heuristic of
our own invention: the plugin author already declared intent, and Halcyon can
honour it.

A `kUnique` actor without `kRespawn` is close to the population this RFC is
concerned with.

## Proposed direction

Three mechanisms, applied by category, instead of one mechanism applied to
everything.

### 1. Prevent (merchants and service providers)

The server refuses to record the death as durable and instructs the killing
client to restore the actor, in the same spirit as vanilla `kProtected`.

Cheapest and safest of the three: nothing has to be recreated, so no identity,
inventory, AI package or quest reference is lost.

Cost: it visibly overrides player agency. A player who empties a shopkeeper's
health bar will see them get back up.

### 2. Scope (non-essential quest owners)

The death is durable, but only inside the Context of the player whose quest
required it. Other players continue to see the actor alive.

This is the mechanism RFC-0001 already built — registry, membership,
persistence, stable identity. Under this RFC it applies to a small, explicitly
classified population rather than every Actor.

Cost: requires the scoped value to win against the ownership and spawn paths,
which it currently does not (see RFC-0001's field notes).

### 3. Respawn (deliberately reconstitutable actors)

The server asks a client to recreate the actor after a delay.

Cost, and the reason this is listed last: a recreated actor is a **new
`ObjectReference`**. Inventory, position, AI packages, faction membership and
any quest alias pointing at the original are not carried over by recreation
alone. `kRespawn` is the game's own signal that an actor is safe to treat this
way; extending it to actors the plugin did not mark is where this becomes
dangerous.

## Relationship to RFC-0001

RFC-0001 is not discarded. Its substrate stands:

- Contexts, membership and the visibility rule;
- persistence with restart-stable entity identity;
- the account-key mapping.

What this RFC revises is the **use case**. "Scope every Actor death" was chosen
because it was easy to observe, not because it was the problem worth solving.
Mechanism 2 above is RFC-0001 applied to a defensible population.

RFC-0001's remaining gap — scoped state losing to `BroadcastActorData` and
`AssignCharacterResponse.IsDead` — becomes less urgent under this framing. If
most deaths should stay global, that leak is only a problem for the narrow
classified set.

## Open questions

These need investigation before any implementation. None are answered here.

### Classification

- Is `kUnique && !kRespawn` a good enough proxy for "world-critical", or does it
  over-select (unique bandit chiefs) and under-select (generic-flagged
  merchants)?
- Merchants are identified in Skyrim by vendor faction membership and a merchant
  container, neither of which the current `NPC_` parser extracts. What would it
  cost to parse them?
- Should server operators be able to override the classification per actor?

### Availability

- `World::GetRecordCollection` is documented as null-checked "when MoPo is on",
  so plugin data may be absent at runtime. What is the correct behaviour when
  the server cannot classify an actor — treat as generic, or as protected?
- Classification depends on the server having the same plugins as the client.
  What happens when it does not?

### Prevention

- Which message tells a client to revive an actor it has already killed? Does
  one exist, or is this new protocol?
- What does the killing player see? An actor that stands back up is a visible
  artefact, and hiding it may be worse than showing it.

### Respawn

- Can a recreated merchant recover its shop inventory, or is that lost with the
  original reference?
- What happens to a player mid-transaction with the actor being replaced?

### Scope of authority

- Vanilla already prevents essential NPC death. Is Halcyon extending a vanilla
  guarantee, or introducing server rules the player cannot predict?

## Non-goals

- Synchronizing vanilla quest state. See AGENTS.md section 10.
- Preventing player-versus-player killing.
- Protecting generic actors.
- Replacing the Context system.

## Implementation status

```text
Nothing in this document is implemented.
NPC classification: not implemented (ACBS flags parsed, never read)
Death prevention: not implemented
Actor respawn: not implemented
```

The mechanisms in RFC-0001 exist and are described in that document.

## Acceptance criteria

A first prototype would be validated when, on a two-client server:

1. a merchant killed by one player still trades with the other;
2. the same merchant still trades after a server restart;
3. a generic actor killed by one player stays dead for both;
4. no quest alias breaks as a result.

Criterion 4 is the one most likely to fail and needs the most evidence.
