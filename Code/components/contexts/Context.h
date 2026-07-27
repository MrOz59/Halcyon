#pragma once

// Halcyon Context system — prototype core types.
//
// Status: Prototype. Implements the minimal model from RFC-0001 only.
// This is not a stable SDK and deliberately covers none of the wider
// Context architecture sketched in HTDS-200 (composition, precedence,
// conflict resolution, lifecycle policies, roles).
//
// See docs/RFC/0001-context-system.md.

#include <cstdint>

namespace Halcyon
{
using ContextId = std::uint64_t;

// Identifies a scoped world object across server runs.
//
// This MUST NOT be an entt handle or any other per-session value: HTDS-200
// section 8 requires Context-scoped identity to stay stable for as long as
// persistence needs it, and a handle is reallocated on every run. Callers are
// responsible for deriving it from something durable -- on the server that is
// the owning plugin's filename plus the record's BaseId, since the numeric
// ModId is assigned in player-connection order and is itself unstable.
//
// See MakeEntityId in ContextIdentity.h.
using EntityId = std::uint64_t;

// Identifies a Player across reconnects. RFC-0001 step 10 requires Context
// membership to survive a restart, so this must not be a Session or a
// connection id, both of which change when a Player reconnects.
using PlayerId = std::uint64_t;

using Revision = std::uint64_t;

// Reserved id meaning "no Context". Server-issued ids start at 1 so that a
// value-initialized ContextId is never mistaken for a valid scope.
inline constexpr ContextId kInvalidContextId = 0;

enum class ContextKind : std::uint8_t
{
    Global,
    Personal,
    Party,
    Instance
};

struct Context
{
    ContextId id{kInvalidContextId};
    ContextKind kind{ContextKind::Global};
    Revision revision{0};
};

// The single scoped component carried by the first prototype: whether one
// quest-critical Actor is dead, as observed from inside one Context.
struct ScopedLifeState
{
    ContextId context{kInvalidContextId};
    EntityId entity{0};
    bool dead{false};
    Revision revision{0};
};
} // namespace Halcyon
