# Network synchronization

This fork keeps the vanilla `v1.8.0` wire format. Synchronization improvements must
therefore change scheduling, delivery policy, validation, or server-side state
management without adding fields to protocol messages.

## State and delivery model

- The server remains authoritative for entity ownership, interest, canonical
  relayed state, inventories, parties, objects, weather, and calendar state. The
  owning client still produces movement and animation input; the server validates
  its ordering before relaying it.
- Discrete transitions (authentication, spawn/remove, ownership, inventory,
  equipment, combat, animation actions, and cell changes) use reliable delivery.
- Movement-only snapshots use the unreliable/no-delay delivery path. A newer
  snapshot supersedes an older one, so queuing stale positions behind a lost packet
  increases latency without improving correctness.
- A movement batch containing animation actions remains reliable. If it arrives
  after a newer movement-only packet, the server relays its actions with the newest
  accepted movement state instead of rolling the entity back.
- Incoming movement is accepted only when its synchronized tick is newer than the
  entity's current tick and is not implausibly far ahead of the server clock.
- Ownership transfer resets that entity's sequencing baseline. This prevents the
  previous owner's higher half-RTT estimate from temporarily blocking movement
  produced by a lower-latency new owner.

## Timing

The transport uses a monotonic clock and nanosecond tick intervals. A 60 Hz server
therefore schedules at 16.666 ms rather than truncating the interval to 16 ms.
Scheduling remains phase-locked and skips missed slots after a stall instead of
issuing burst updates.

The server refreshes client clock estimates every two seconds. Client clock
corrections are monotonic, preventing interpolation timestamps from moving backward
when the measured RTT changes.

Client ticks are milliseconds on the server's `steady_clock` epoch, which is the
value `Server::GetTick()` broadcasts and `SynchronizedClock` anchors to. The
server-side plausibility bound compares against that same base; switching either
side to a wall clock would reject every movement packet.

`SynchronizedClock::GetCurrentTick()` returns `0` until the first server time sync
arrives, so consumers that subtract an interpolation delay must clamp rather than
let the unsigned subtraction wrap.

Movement input is sampled at 20 Hz. The server evaluates dirty movement on every
server tick, while continuing to send nothing when no entity changed.

## Interest management

`Player::CellIdComponent` represents the client's loaded region:

- `Cell` is the current cell;
- `WorldSpaceId` is the current world space;
- `CenterCoords` is the center of the loaded exterior grid.

An actor entity's `CellIdComponent::CenterCoords`, in contrast, represents the
actor's actual grid coordinate. `EnterExteriorCellRequest::CurrentCoords` must not
replace the player's loaded-grid center.

When a player shifts the exterior grid or changes between an exterior and interior
cell, the server compares the old and new interest regions. It reliably spawns
entities entering visibility and removes entities leaving it. Dragon visibility uses
the extended dragon grid in both movement filtering and cell-transition broadcasts.

## Vanilla client compatibility

Unmodified `v1.8.0` clients still connect and play against this server. Nothing in
the message set changed: no field, opcode, or serialization was touched, and the
transport framing is untouched as well.

Delivery reliability is a per-send flag on the sender's side. The receiver calls the
same receive path and cannot observe, or depend on, how a packet was sent, so moving
movement snapshots to the unreliable channel is transparent to a vanilla client.
More frequent clock synchronization reuses the existing server-time message, which
vanilla clients already handle on arrival without assuming a period.

One behavioural difference is worth knowing. A vanilla client lacks the monotonic
clock correction, so a sharp drop in measured RTT can make it emit a tick that moves
backward. This server treats that movement packet as stale and drops it, which shows
up as a brief stutter rather than a desync or a disconnect: the next packet with a
higher tick is accepted normally. The original server applied the same bad sample
instead of rejecting it.

## Remaining protocol-level limits

The vanilla protocol has no state revision numbers, snapshot acknowledgements, or
full-world reconciliation transaction. Reliable event ordering and canonical state
on assignment cover normal play, but a future protocol revision could improve
recovery further with per-entity revisions, explicit snapshot baselines, and
acknowledged interest generations.

## Validation checklist

For an in-game multiplayer pass, test at least two clients while applying latency,
jitter, and packet loss:

1. Walk repeatedly across exterior grid boundaries and confirm actors disappear and
   reappear at the same boundaries on both clients.
2. Transition exterior-to-interior and interior-to-exterior with actors on both
   sides of the transition.
3. Trigger combat animations while moving under packet loss; actions must not be
   lost and positions must not roll backward.
4. Transfer NPC ownership while movement packets are in flight.
5. Compare the displayed client RTT/loss statistics with movement smoothness at
   30 Hz and 60 Hz server modes.
