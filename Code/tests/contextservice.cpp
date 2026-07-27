// Tests for the ContextService integration logic (RFC-0001 step 2).
//
// ContextService itself lives in Code/server and depends on World, Player and
// spdlog, none of which TPTests links against. These tests therefore exercise a
// faithful reimplementation of its decision logic over the real
// ContextRegistry, so the rules below are validated even though the production
// type is not linked here.
//
// The reimplementation is deliberately small; if ContextService gains real
// behaviour it should move into a linkable unit and be tested directly.

#include "contexts/ContextRegistry.h"

#include <map>
#include <optional>

#include <catch2/catch.hpp>

using namespace Halcyon;

namespace
{
// Mirrors ContextService: maps the server's volatile per-process player id to a
// stable Context-side PlayerId, owns Personal Contexts, and gates everything
// behind the prototype flag.
class ContextServiceLogic
{
public:
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool aEnabled) { m_enabled = aEnabled; }

    ContextId EnsurePersonalContext(std::uint32_t aServerPlayerId)
    {
        if (!m_enabled)
            return kInvalidContextId;

        auto playerId = FindPlayerId(aServerPlayerId);
        if (!playerId)
        {
            playerId = m_nextPlayerId++;
            m_playerIds.emplace(aServerPlayerId, *playerId);
        }

        const auto existing = m_personalContexts.find(*playerId);
        if (existing != m_personalContexts.end())
        {
            m_registry.AddMember(existing->second, *playerId);
            return existing->second;
        }

        const ContextId context = m_registry.CreateContext(ContextKind::Personal);
        m_registry.AddMember(context, *playerId);
        m_personalContexts.emplace(*playerId, context);

        return context;
    }

    // Deliberately keeps the server-id mapping; see ContextService.cpp.
    void OnPlayerDisconnected(std::uint32_t) {}

    std::optional<ContextId> GetPersonalContext(std::uint32_t aServerPlayerId) const
    {
        const auto playerId = FindPlayerId(aServerPlayerId);
        if (!playerId)
            return std::nullopt;

        const auto it = m_personalContexts.find(*playerId);
        if (it == m_personalContexts.end())
            return std::nullopt;

        return it->second;
    }

    bool RecordLifeState(std::uint32_t aServerPlayerId, EntityId aEntity, bool aDead)
    {
        if (!m_enabled)
            return false;

        const auto context = GetPersonalContext(aServerPlayerId);
        if (!context)
            return false;

        return m_registry.SetLifeState(*context, aEntity, aDead, m_nextRevision++) == MutationResult::Applied;
    }

    std::optional<bool> GetObservedLifeState(std::uint32_t aServerPlayerId, EntityId aEntity) const
    {
        if (!m_enabled)
            return std::nullopt;

        const auto playerId = FindPlayerId(aServerPlayerId);
        if (!playerId)
            return std::nullopt;

        for (const ContextId context : m_registry.GetPlayerContexts(*playerId))
        {
            if (const auto state = m_registry.GetLifeState(context, aEntity))
                return state->dead;
        }

        return std::nullopt;
    }

private:
    std::optional<PlayerId> FindPlayerId(std::uint32_t aServerPlayerId) const
    {
        const auto it = m_playerIds.find(aServerPlayerId);
        if (it == m_playerIds.end())
            return std::nullopt;

        return it->second;
    }

    bool m_enabled{false};
    ContextRegistry m_registry;
    PlayerId m_nextPlayerId{1};
    std::map<std::uint32_t, PlayerId> m_playerIds;
    std::map<PlayerId, ContextId> m_personalContexts;
    Revision m_nextRevision{1};
};

constexpr EntityId kActor = 0x2000;
constexpr std::uint32_t kServerPlayerA = 1;
constexpr std::uint32_t kServerPlayerB = 2;
} // namespace

TEST_CASE("Disabled prototype records and reports nothing", "[contextservice]")
{
    ContextServiceLogic service;
    REQUIRE_FALSE(service.IsEnabled());

    // Every entry point must be inert, so the legacy replication path stays
    // the only source of Actor life state.
    REQUIRE(service.EnsurePersonalContext(kServerPlayerA) == kInvalidContextId);
    REQUIRE_FALSE(service.GetPersonalContext(kServerPlayerA).has_value());
    REQUIRE_FALSE(service.RecordLifeState(kServerPlayerA, kActor, true));
    REQUIRE_FALSE(service.GetObservedLifeState(kServerPlayerA, kActor).has_value());
}

TEST_CASE("Enabling the prototype does not retroactively create contexts", "[contextservice]")
{
    ContextServiceLogic service;

    REQUIRE(service.EnsurePersonalContext(kServerPlayerA) == kInvalidContextId);

    service.SetEnabled(true);

    // The earlier call was inert, so the Player still has no Context until it
    // is requested again.
    REQUIRE_FALSE(service.GetPersonalContext(kServerPlayerA).has_value());
    REQUIRE(service.EnsurePersonalContext(kServerPlayerA) != kInvalidContextId);
}

TEST_CASE("Each player gets one distinct personal context", "[contextservice]")
{
    ContextServiceLogic service;
    service.SetEnabled(true);

    const ContextId contextA = service.EnsurePersonalContext(kServerPlayerA);
    const ContextId contextB = service.EnsurePersonalContext(kServerPlayerB);

    REQUIRE(contextA != kInvalidContextId);
    REQUIRE(contextB != kInvalidContextId);
    REQUIRE(contextA != contextB);

    // Repeated calls are idempotent rather than allocating a second Context.
    REQUIRE(service.EnsurePersonalContext(kServerPlayerA) == contextA);
}

TEST_CASE("Recording requires an established context", "[contextservice]")
{
    ContextServiceLogic service;
    service.SetEnabled(true);

    // No EnsurePersonalContext call yet: the write must be refused rather than
    // silently creating a scope.
    REQUIRE_FALSE(service.RecordLifeState(kServerPlayerA, kActor, true));

    service.EnsurePersonalContext(kServerPlayerA);
    REQUIRE(service.RecordLifeState(kServerPlayerA, kActor, true));
}

TEST_CASE("RFC-0001 divergence through the service layer", "[contextservice]")
{
    ContextServiceLogic service;
    service.SetEnabled(true);

    service.EnsurePersonalContext(kServerPlayerA);
    service.EnsurePersonalContext(kServerPlayerB);

    // Player A kills the Actor.
    REQUIRE(service.RecordLifeState(kServerPlayerA, kActor, true));

    // A observes the death; B is unaffected and falls back to base game data.
    const auto observedByA = service.GetObservedLifeState(kServerPlayerA, kActor);
    REQUIRE(observedByA.has_value());
    REQUIRE(*observedByA);

    REQUIRE_FALSE(service.GetObservedLifeState(kServerPlayerB, kActor).has_value());
}

TEST_CASE("Context survives disconnect within one server run", "[contextservice]")
{
    ContextServiceLogic service;
    service.SetEnabled(true);

    const ContextId context = service.EnsurePersonalContext(kServerPlayerA);
    REQUIRE(service.RecordLifeState(kServerPlayerA, kActor, true));

    service.OnPlayerDisconnected(kServerPlayerA);

    // The Context and its scoped state outlive the Session, as RFC-0001
    // step 10 requires.
    REQUIRE(service.GetPersonalContext(kServerPlayerA) == context);

    // Reconnecting under a NEW server id allocates a different Context, because
    // nothing maps the returning Player back to the old one. This documents a
    // real prototype limitation: Player::GetId is a per-process counter, so
    // RFC-0001 step 10 needs a persistent account identity that does not exist
    // yet. A restart loses the association entirely.
    constexpr std::uint32_t reconnectedId = 99;
    const ContextId afterReconnect = service.EnsurePersonalContext(reconnectedId);
    REQUIRE(afterReconnect != context);
    REQUIRE_FALSE(service.GetObservedLifeState(reconnectedId, kActor).has_value());
}

TEST_CASE("Reconnect under the same server id rejoins the context", "[contextservice]")
{
    ContextServiceLogic service;
    service.SetEnabled(true);

    const ContextId context = service.EnsurePersonalContext(kServerPlayerA);
    REQUIRE(service.RecordLifeState(kServerPlayerA, kActor, true));

    service.OnPlayerDisconnected(kServerPlayerA);

    // Player::GetId is a per-process counter, so this only happens within one
    // server run; it is not the persistence path.
    REQUIRE(service.EnsurePersonalContext(kServerPlayerA) == context);

    const auto observed = service.GetObservedLifeState(kServerPlayerA, kActor);
    REQUIRE(observed.has_value());
    REQUIRE(*observed);
}
