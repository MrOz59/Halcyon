// Tests for Context persistence (RFC-0001 steps 9-10).
//
// These cover the serialization format and the registry restore path. They do
// not establish that a real server restart preserves gameplay state; that
// requires two clients and a live server.

#include "contexts/ContextStore.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include <catch2/catch.hpp>

using namespace Halcyon;

namespace
{
constexpr EntityId kActor = 0x3000;

ContextSnapshot MakeSnapshot()
{
    ContextSnapshot snapshot;
    snapshot.nextContextId = 3;
    snapshot.nextRevision = 8;
    snapshot.memberships.push_back({"alice", 1, ContextKind::Personal});
    snapshot.memberships.push_back({"bob", 2, ContextKind::Personal});
    snapshot.lifeStates.push_back({1, kActor, true, 5});
    snapshot.lifeStates.push_back({2, kActor, false, 6});
    return snapshot;
}

// Unique per test to avoid cross-test interference.
std::string TempPath(const char* acName)
{
    return (std::filesystem::temp_directory_path() / acName).string();
}
} // namespace

TEST_CASE("Snapshot survives a serialize/deserialize round trip", "[contextstore]")
{
    const ContextSnapshot original = MakeSnapshot();

    std::string payload;
    REQUIRE(ContextStore::Serialize(original, payload));

    ContextSnapshot restored;
    REQUIRE(ContextStore::Deserialize(payload, restored));

    REQUIRE(restored.nextContextId == original.nextContextId);
    REQUIRE(restored.nextRevision == original.nextRevision);
    REQUIRE(restored.memberships.size() == 2);
    REQUIRE(restored.lifeStates.size() == 2);

    REQUIRE(restored.memberships[0].accountKey == "alice");
    REQUIRE(restored.memberships[0].context == 1);
    REQUIRE(restored.memberships[1].accountKey == "bob");

    // The divergence itself must survive: same Actor, opposite states.
    REQUIRE(restored.lifeStates[0].entity == kActor);
    REQUIRE(restored.lifeStates[0].dead);
    REQUIRE(restored.lifeStates[1].entity == kActor);
    REQUIRE_FALSE(restored.lifeStates[1].dead);
}

TEST_CASE("An account key containing whitespace is refused", "[contextstore]")
{
    ContextSnapshot snapshot;
    snapshot.memberships.push_back({"two words", 1, ContextKind::Personal});

    std::string payload = "untouched";

    // Writing it would produce a file that cannot be read back, so the write
    // must fail rather than silently corrupt the store.
    REQUIRE_FALSE(ContextStore::Serialize(snapshot, payload));
    REQUIRE(payload == "untouched");
}

TEST_CASE("Malformed input is rejected without partial application", "[contextstore]")
{
    ContextSnapshot snapshot = MakeSnapshot();
    const auto originalCount = snapshot.memberships.size();

    SECTION("empty input")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("", snapshot));
    }

    SECTION("wrong magic")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("something-else 1\nnext 1 1\n", snapshot));
    }

    SECTION("unsupported version")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 99\nnext 1 1\n", snapshot));
    }

    SECTION("missing next record")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nmember alice 1 1\n", snapshot));
    }

    SECTION("unknown record type")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 2 2\nfuture alice 1\n", snapshot));
    }

    SECTION("truncated member record")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 2 2\nmember alice\n", snapshot));
    }

    SECTION("invalid context kind")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 2 2\nmember alice 1 77\n", snapshot));
    }

    SECTION("context id beyond the allocator")
    {
        // Restoring this would let CreateContext reissue a live id.
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 2 2\nmember alice 5 1\n", snapshot));
    }

    SECTION("revision beyond the allocator")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 3 2\nlife 1 100 1 9\n", snapshot));
    }

    SECTION("invalid dead flag")
    {
        REQUIRE_FALSE(ContextStore::Deserialize("halcyon-context-store 1\nnext 3 9\nlife 1 100 7 1\n", snapshot));
    }

    // In every case the caller's snapshot is left exactly as it was.
    REQUIRE(snapshot.memberships.size() == originalCount);
    REQUIRE(snapshot.nextContextId == 3);
}

TEST_CASE("Snapshot survives a file round trip", "[contextstore]")
{
    const std::string path = TempPath("halcyon-context-roundtrip.txt");
    std::filesystem::remove(path);

    REQUIRE(ContextStore::SaveToFile(MakeSnapshot(), path));
    REQUIRE(ContextStore::FileExists(path));

    ContextSnapshot restored;
    REQUIRE(ContextStore::LoadFromFile(path, restored));
    REQUIRE(restored.memberships.size() == 2);
    REQUIRE(restored.lifeStates.size() == 2);

    std::filesystem::remove(path);
}

TEST_CASE("A missing store is reported rather than treated as empty", "[contextstore]")
{
    const std::string path = TempPath("halcyon-context-absent.txt");
    std::filesystem::remove(path);

    REQUIRE_FALSE(ContextStore::FileExists(path));

    ContextSnapshot snapshot;
    REQUIRE_FALSE(ContextStore::LoadFromFile(path, snapshot));
}

TEST_CASE("A corrupt store does not overwrite the previous one on load", "[contextstore]")
{
    const std::string path = TempPath("halcyon-context-corrupt.txt");

    {
        std::ofstream file(path, std::ios::trunc);
        file << "halcyon-context-store 1\nnext 3 9\nmember alice 1 1\ngarbage\n";
    }

    ContextSnapshot snapshot;
    REQUIRE_FALSE(ContextStore::LoadFromFile(path, snapshot));

    // Nothing was applied, so the server starts empty rather than half-loaded.
    REQUIRE(snapshot.memberships.empty());
    REQUIRE(snapshot.lifeStates.empty());

    std::filesystem::remove(path);
}

TEST_CASE("Registry restores persisted contexts without reissuing ids", "[contextstore]")
{
    ContextRegistry registry;

    REQUIRE(registry.RestoreContext(5, ContextKind::Personal, 3));
    REQUIRE(registry.HasContext(5));

    // Allocation must continue past every restored id.
    const ContextId fresh = registry.CreateContext(ContextKind::Personal);
    REQUIRE(fresh > 5);

    // A duplicate or invalid id is refused instead of colliding.
    REQUIRE_FALSE(registry.RestoreContext(5, ContextKind::Personal, 0));
    REQUIRE_FALSE(registry.RestoreContext(kInvalidContextId, ContextKind::Personal, 0));
}

TEST_CASE("Restored scoped state reproduces the RFC-0001 divergence", "[contextstore]")
{
    ContextRegistry registry;

    // Rebuild the state a previous run would have persisted.
    REQUIRE(registry.RestoreContext(1, ContextKind::Personal, 5));
    REQUIRE(registry.RestoreContext(2, ContextKind::Personal, 0));
    REQUIRE(registry.AddMember(1, 1));
    REQUIRE(registry.AddMember(2, 2));
    REQUIRE(registry.RestoreLifeState({1, kActor, true, 5}));

    // Step 10: A sees the Actor dead, B sees it alive.
    const auto stateA = registry.GetLifeState(1, kActor);
    REQUIRE(stateA.has_value());
    REQUIRE(stateA->dead);
    REQUIRE(registry.IsVisibleTo(*stateA, 1));
    REQUIRE_FALSE(registry.IsVisibleTo(*stateA, 2));

    REQUIRE_FALSE(registry.GetLifeState(2, kActor).has_value());
    REQUIRE(registry.GetVisibleLifeStates(2).empty());
}

TEST_CASE("Scoped state for an unknown context is refused", "[contextstore]")
{
    ContextRegistry registry;

    // Would otherwise create an orphan entry no Player can ever observe.
    REQUIRE_FALSE(registry.RestoreLifeState({42, kActor, true, 1}));
}

TEST_CASE("Restored state still rejects stale live mutations", "[contextstore]")
{
    ContextRegistry registry;

    REQUIRE(registry.RestoreContext(1, ContextKind::Personal, 5));
    REQUIRE(registry.RestoreLifeState({1, kActor, true, 5}));

    // A revision at or below the restored one must not resurrect the Actor
    // after a restart.
    REQUIRE(registry.SetLifeState(1, kActor, false, 5) == MutationResult::RejectedStaleRevision);
    REQUIRE(registry.GetLifeState(1, kActor)->dead);

    REQUIRE(registry.SetLifeState(1, kActor, false, 6) == MutationResult::Applied);
}
