// Tests for stable Context entity identity (RFC-0001).
//
// These pin down the property the second live test showed was missing: an
// EntityId must identify the same game record across server restarts, where
// entt handles and numeric ModIds do not.

#include "contexts/ContextIdentity.h"

#include <set>
#include <string>

#include <catch2/catch.hpp>

using namespace Halcyon;

TEST_CASE("Identity is stable for the same plugin and base id", "[contextidentity]")
{
    const EntityId first = MakeEntityId("Skyrim.esm", 0x1337);
    const EntityId second = MakeEntityId("Skyrim.esm", 0x1337);

    REQUIRE(first == second);
    REQUIRE(first != 0);
}

TEST_CASE("Identity is computed at compile time", "[contextidentity]")
{
    // constexpr guarantees the value cannot drift with runtime state, and that
    // the same build always produces the same id.
    static constexpr EntityId id = MakeEntityId("Skyrim.esm", 42);
    static_assert(id != 0, "a valid plugin must produce a usable id");
    STATIC_REQUIRE(id == MakeEntityId("Skyrim.esm", 42));
}

TEST_CASE("Different records produce different identities", "[contextidentity]")
{
    std::set<EntityId> seen;

    seen.insert(MakeEntityId("Skyrim.esm", 1));
    seen.insert(MakeEntityId("Skyrim.esm", 2));
    seen.insert(MakeEntityId("Dawnguard.esm", 1));
    seen.insert(MakeEntityId("Dawnguard.esm", 2));

    REQUIRE(seen.size() == 4);
}

TEST_CASE("Neighbouring plugin names stay distinct", "[contextidentity]")
{
    // The BaseId is folded in as fixed-width bytes, so these are distinct
    // regardless of the separator; this guards the encoding as a whole rather
    // than any one part of it.
    REQUIRE(MakeEntityId("ab", 1) != MakeEntityId("a", 1));
    REQUIRE(MakeEntityId("Skyrim.esm", 0) != MakeEntityId("Skyrim.esm|", 0));
}

TEST_CASE("An absent plugin name yields no identity", "[contextidentity]")
{
    // 0 is the caller's signal to refuse to persist, rather than a hash that
    // would collide across unrelated records.
    REQUIRE(MakeEntityId("", 1) == 0);
    REQUIRE(MakeEntityId("", 0) == 0);
}

TEST_CASE("Identity is independent of the numeric ModId", "[contextidentity]")
{
    // ModsComponent assigns ModId from a counter in player-connection order,
    // so the same plugin can be 1 in one run and 7 in the next. Identity is
    // derived from the filename precisely so that this does not matter, which
    // is exactly the bug the second live test exposed.
    const EntityId beforeRestart = MakeEntityId("Skyrim.esm", 0x100045);
    const EntityId afterRestart = MakeEntityId("Skyrim.esm", 0x100045);

    REQUIRE(beforeRestart == afterRestart);
}

TEST_CASE("Comparison is case sensitive", "[contextidentity]")
{
    // Documents current behaviour rather than endorsing it: Windows and Linux
    // clients may report differing case for one plugin, so callers must
    // normalise before calling.
    REQUIRE(MakeEntityId("Skyrim.esm", 1) != MakeEntityId("skyrim.esm", 1));
}

TEST_CASE("Identity survives a store round trip", "[contextidentity]")
{
    // The id is written to disk as a decimal integer, so it must survive that
    // representation without truncation.
    const EntityId original = MakeEntityId("Dragonborn.esm", 0xABCDEF);

    const std::string serialized = std::to_string(original);
    const EntityId restored = std::stoull(serialized);

    REQUIRE(restored == original);
}
