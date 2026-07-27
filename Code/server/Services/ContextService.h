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
#include <contexts/ContextStore.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

struct World;
struct Player;
struct PlayerJoinEvent;
struct PlayerLeaveEvent;

struct ContextService
{
    ContextService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~ContextService() noexcept = default;

    TP_NOCOPYMOVE(ContextService);

    // The prototype is opt-in, driven by the Halcyon:bEnableContexts setting in
    // STServer.ini. While disabled the service records nothing and reports no
    // scoped state, which keeps the legacy global broadcast the only source of
    // Actor life state.
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    // Applies the current setting value, loading persisted state on the
    // transition to enabled. Called when Players join, because settings are
    // read from the ini after services are constructed.
    void SyncEnabledFromSettings() noexcept;

    // Overrides the setting. Intended for tests and for callers that manage the
    // flag directly; normal operation goes through SyncEnabledFromSettings.
    void SetEnabled(bool aEnabled) noexcept { m_enabled = aEnabled; }

    // Assigns the Player a Personal Context, creating one on first use.
    // Returns kInvalidContextId when the prototype is disabled.
    Halcyon::ContextId EnsurePersonalContext(Player& aPlayer) noexcept;

    [[nodiscard]] std::optional<Halcyon::ContextId> GetPersonalContext(const Player& acPlayer) const noexcept;

    // Records an Actor life state inside the acting Player's Personal Context.
    // Returns false when the prototype is disabled, the Player has no Context,
    // or the revision is stale.
    bool RecordLifeState(const Player& acPlayer, Halcyon::EntityId aEntity, bool aDead) noexcept;

    // Whether aObserver may observe aEntity's scoped life state, and what that
    // state is. Returns nullopt when no Context of the observer recorded one,
    // meaning the caller must fall back to base game data.
    [[nodiscard]] std::optional<bool> GetObservedLifeState(const Player& acObserver, Halcyon::EntityId aEntity) const noexcept;

    // Drops per-Session state for a leaving Player. The Context and its scoped
    // state are intentionally kept.
    void OnPlayerDisconnected(const Player& acPlayer) noexcept;

    // Persistence. Both are no-ops while the prototype is disabled.
    //
    // Save is called on Player leave and at shutdown rather than on every
    // mutation: the prototype favours a simple, obviously-correct write over
    // the transactional model HTDS-170 describes.
    bool Save() noexcept;
    bool Load() noexcept;

    void SetStorePath(std::string aPath) noexcept { m_storePath = std::move(aPath); }
    [[nodiscard]] const std::string& GetStorePath() const noexcept { return m_storePath; }

    [[nodiscard]] const Halcyon::ContextRegistry& GetRegistry() const noexcept { return m_registry; }

protected:
    void OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept;

    // Keeps the Context and its scoped state: RFC-0001 step 10 requires them to
    // outlive the Session. Nothing persists them across a restart yet.
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

private:
    // The identity a returning Player is recognised by across restarts.
    //
    // WARNING: this is derived from the username supplied in
    // AuthenticationRequest, and the server does not currently enforce
    // username uniqueness or verify any credential. Two Players claiming the
    // same name therefore share a Personal Context and its scoped state. That
    // is acceptable for a local two-client prototype and is NOT acceptable on
    // a public server; a real account identity must replace this before the
    // Context system carries anything durable. See the RFC status section.
    [[nodiscard]] static std::string MakeAccountKey(const Player& acPlayer) noexcept;

    // Maps a live Player to the Context-side identity. Player::GetId is a
    // per-process counter that restarts with the server, so it addresses the
    // Session while the account key addresses the durable identity.
    [[nodiscard]] std::optional<Halcyon::PlayerId> FindPlayerId(const Player& acPlayer) const noexcept;

    World& m_world;
    bool m_enabled{false};
    std::string m_storePath{"config/halcyon-contexts.txt"};

    Halcyon::ContextRegistry m_registry;

    Halcyon::PlayerId m_nextPlayerId{1};
    std::map<uint32_t, Halcyon::PlayerId> m_playerIds;
    std::map<Halcyon::PlayerId, Halcyon::ContextId> m_personalContexts;

    // Durable identity: account key <-> PlayerId. Populated on join and from
    // the persisted snapshot on load.
    std::map<std::string, Halcyon::PlayerId> m_accountKeys;
    std::map<Halcyon::PlayerId, std::string> m_playerAccountKeys;

    // Monotonic per-service revision. Sufficient while one server owns all
    // mutations; a distributed writer would need a different scheme.
    Halcyon::Revision m_nextRevision{1};

    entt::scoped_connection m_playerJoinConnection;
    entt::scoped_connection m_playerLeaveConnection;
};
