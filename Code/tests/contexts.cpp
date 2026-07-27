// Tests for the Halcyon Context prototype (RFC-0001).
//
// These cover the server-side state model only. The full RFC-0001 acceptance
// criteria require two real clients and a server restart, which is outside
// what a unit test can establish; see docs/RFC/0001-context-system.md.

#include "contexts/ContextRegistry.h"

#include <algorithm>

#include <catch2/catch.hpp>

using namespace Halcyon;

namespace
{
// Stands in for the quest-critical Actor of the prototype scenario.
constexpr EntityId kActor = 0x1000;

constexpr PlayerId kPlayerA = 1;
constexpr PlayerId kPlayerB = 2;
} // namespace

TEST_CASE("Context ids are server issued and never zero", "[contexts]")
{
    ContextRegistry registry;

    const ContextId first = registry.CreateContext(ContextKind::Personal);
    const ContextId second = registry.CreateContext(ContextKind::Personal);

    REQUIRE(first != kInvalidContextId);
    REQUIRE(second != kInvalidContextId);
    REQUIRE(first != second);
    REQUIRE(registry.GetContextCount() == 2);
}

TEST_CASE("Unknown contexts are reported rather than silently created", "[contexts]")
{
    ContextRegistry registry;

    REQUIRE_FALSE(registry.HasContext(1));
    REQUIRE_FALSE(registry.GetContext(1).has_value());
    REQUIRE_FALSE(registry.AddMember(1, kPlayerA));
    REQUIRE(registry.SetLifeState(1, kActor, true, 1) == MutationResult::RejectedUnknownContext);
}

TEST_CASE("Membership tracks both directions", "[contexts]")
{
    ContextRegistry registry;
    const ContextId context = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.AddMember(context, kPlayerA));
    REQUIRE(registry.IsMember(context, kPlayerA));
    REQUIRE(registry.GetPlayerContexts(kPlayerA) == std::vector<ContextId>{context});
    REQUIRE(registry.GetMembers(context) == std::vector<PlayerId>{kPlayerA});

    REQUIRE(registry.RemoveMember(context, kPlayerA));
    REQUIRE_FALSE(registry.IsMember(context, kPlayerA));
    REQUIRE(registry.GetPlayerContexts(kPlayerA).empty());
    REQUIRE(registry.GetMembers(context).empty());

    // Removing a non-member reports failure instead of corrupting the index.
    REQUIRE_FALSE(registry.RemoveMember(context, kPlayerA));
}

TEST_CASE("A player may belong to several contexts", "[contexts]")
{
    ContextRegistry registry;
    const ContextId global = registry.CreateContext(ContextKind::Global);
    const ContextId personal = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.AddMember(global, kPlayerA));
    REQUIRE(registry.AddMember(personal, kPlayerA));

    const auto contexts = registry.GetPlayerContexts(kPlayerA);
    REQUIRE(contexts.size() == 2);
    REQUIRE(std::find(contexts.begin(), contexts.end(), global) != contexts.end());
    REQUIRE(std::find(contexts.begin(), contexts.end(), personal) != contexts.end());
}

TEST_CASE("Absent life state is distinct from alive", "[contexts]")
{
    ContextRegistry registry;
    const ContextId context = registry.CreateContext(ContextKind::Personal);

    // Nothing recorded yet: the caller must fall back to base game data
    // rather than assume the Actor is alive.
    REQUIRE_FALSE(registry.GetLifeState(context, kActor).has_value());

    REQUIRE(registry.SetLifeState(context, kActor, false, 1) == MutationResult::Applied);

    const auto state = registry.GetLifeState(context, kActor);
    REQUIRE(state.has_value());
    REQUIRE_FALSE(state->dead);
}

TEST_CASE("Stale and replayed revisions are rejected", "[contexts]")
{
    ContextRegistry registry;
    const ContextId context = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.SetLifeState(context, kActor, true, 5) == MutationResult::Applied);

    // An older revision must not resurrect the Actor.
    REQUIRE(registry.SetLifeState(context, kActor, false, 4) == MutationResult::RejectedStaleRevision);
    REQUIRE(registry.GetLifeState(context, kActor)->dead);

    // Neither may the same revision be replayed.
    REQUIRE(registry.SetLifeState(context, kActor, false, 5) == MutationResult::RejectedStaleRevision);
    REQUIRE(registry.GetLifeState(context, kActor)->dead);

    // A newer revision still applies.
    REQUIRE(registry.SetLifeState(context, kActor, false, 6) == MutationResult::Applied);
    REQUIRE_FALSE(registry.GetLifeState(context, kActor)->dead);
}

TEST_CASE("RFC-0001 scenario: one actor diverges between personal contexts", "[contexts]")
{
    ContextRegistry registry;

    // Steps 3-4: A and B have separate Personal Contexts.
    const ContextId contextA = registry.CreateContext(ContextKind::Personal);
    const ContextId contextB = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.AddMember(contextA, kPlayerA));
    REQUIRE(registry.AddMember(contextB, kPlayerB));

    // Steps 5-6: A kills the Actor; the server stores it in A's Context only.
    REQUIRE(registry.SetLifeState(contextA, kActor, true, 1) == MutationResult::Applied);

    // A observes the death.
    const auto stateA = registry.GetLifeState(contextA, kActor);
    REQUIRE(stateA.has_value());
    REQUIRE(stateA->dead);
    REQUIRE(registry.IsVisibleTo(*stateA, kPlayerA));

    // Step 7: B does not receive it, and B's own Context is untouched.
    REQUIRE_FALSE(registry.IsVisibleTo(*stateA, kPlayerB));
    REQUIRE_FALSE(registry.GetLifeState(contextB, kActor).has_value());

    const auto visibleToB = registry.GetVisibleLifeStates(kPlayerB);
    REQUIRE(visibleToB.empty());

    const auto visibleToA = registry.GetVisibleLifeStates(kPlayerA);
    REQUIRE(visibleToA.size() == 1);
    REQUIRE(visibleToA.front().context == contextA);
    REQUIRE(visibleToA.front().dead);
}

TEST_CASE("Scoped state is keyed per context, not per entity", "[contexts]")
{
    ContextRegistry registry;
    const ContextId contextA = registry.CreateContext(ContextKind::Personal);
    const ContextId contextB = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.SetLifeState(contextA, kActor, true, 1) == MutationResult::Applied);
    REQUIRE(registry.SetLifeState(contextB, kActor, false, 1) == MutationResult::Applied);

    // The same Actor holds opposite states in two Contexts simultaneously.
    REQUIRE(registry.GetLifeState(contextA, kActor)->dead);
    REQUIRE_FALSE(registry.GetLifeState(contextB, kActor)->dead);

    // Revisions are tracked per (context, entity) pair, so a high revision in
    // one Context does not block a low one in another.
    REQUIRE(registry.SetLifeState(contextA, kActor, false, 9) == MutationResult::Applied);
    REQUIRE(registry.SetLifeState(contextB, kActor, true, 2) == MutationResult::Applied);
}

TEST_CASE("Losing membership hides scoped state", "[contexts]")
{
    ContextRegistry registry;
    const ContextId context = registry.CreateContext(ContextKind::Personal);

    REQUIRE(registry.AddMember(context, kPlayerA));
    REQUIRE(registry.SetLifeState(context, kActor, true, 1) == MutationResult::Applied);
    REQUIRE(registry.GetVisibleLifeStates(kPlayerA).size() == 1);

    REQUIRE(registry.RemoveMember(context, kPlayerA));

    // The state survives, but is no longer replicated to the former member.
    REQUIRE(registry.GetVisibleLifeStates(kPlayerA).empty());
    REQUIRE(registry.GetLifeState(context, kActor)->dead);
}
