#pragma once

// Halcyon Context system — server integration.
//
// Status: Prototype. Implements step 2 of the RFC-0001 sequence: scoped Actor
// life state is recorded per Context, and the set of recipients for a scoped
// value is computed from Context membership.
//
// This service does not replace the existing replication path. It runs beside
// it and is inert unless explicitly enabled, so that the legacy behaviour in
// CharacterService/GameServer is preserved exactly while the prototype is
// being validated.
//
// See docs/RFC/0001-context-system.md.

#include <contexts/ContextRegistry.h>

#include <cstdint>
#include <map>
#include <optional>

struct World;
struct Player;

struct ContextService
{
    explicit ContextService(World& aWorld) noexcept;
    ~ContextService() noexcept = default;

    TP_NOCOPYMOVE(ContextService);

    // The prototype is opt-in. While disabled the service records nothing and
    // reports no scoped state, which keeps the legacy global broadcast the only
    // source of Actor life state.
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    void SetEnabled(bool aEnabled) noexcept { m_enabled = aEnabled; }

    // Assigns the Player a Personal Context, creating one on first use.
    // Returns kInvalidContextId when the prototype is disabled.
    Halcyon::ContextId EnsurePersonalContext(Player& aPlayer) noexcept;

    // Drops the in-memory association for a disconnecting Player. The Context
    // and its scoped state are intentionally kept: RFC-0001 step 10 requires
    // them to outlive the Session. Nothing persists them across a restart yet.
    void OnPlayerDisconnected(const Player& acPlayer) noexcept;

    [[nodiscard]] std::optional<Halcyon::ContextId> GetPersonalContext(const Player& acPlayer) const noexcept;

    // Records an Actor life state inside the acting Player's Personal Context.
    // Returns false when the prototype is disabled, the Player has no Context,
    // or the revision is stale.
    bool RecordLifeState(const Player& acPlayer, Halcyon::EntityId aEntity, bool aDead) noexcept;

    // Whether aObserver may observe aEntity's scoped life state, and what that
    // state is. Returns nullopt when no Context of the observer recorded one,
    // meaning the caller must fall back to base game data.
    [[nodiscard]] std::optional<bool> GetObservedLifeState(const Player& acObserver, Halcyon::EntityId aEntity) const noexcept;

    [[nodiscard]] const Halcyon::ContextRegistry& GetRegistry() const noexcept { return m_registry; }

private:
    // Maps a live Player to the stable Context-side identity. Player::GetId is
    // a per-process counter that restarts with the server, so it cannot be the
    // PlayerId itself; this indirection is where a persistent account id will
    // be substituted once persistence exists.
    [[nodiscard]] std::optional<Halcyon::PlayerId> FindPlayerId(const Player& acPlayer) const noexcept;

    World& m_world;
    bool m_enabled{false};

    Halcyon::ContextRegistry m_registry;

    Halcyon::PlayerId m_nextPlayerId{1};
    std::map<uint32_t, Halcyon::PlayerId> m_playerIds;
    std::map<Halcyon::PlayerId, Halcyon::ContextId> m_personalContexts;

    // Monotonic per-service revision. Sufficient while one server owns all
    // mutations; a distributed writer would need a different scheme.
    Halcyon::Revision m_nextRevision{1};
};
