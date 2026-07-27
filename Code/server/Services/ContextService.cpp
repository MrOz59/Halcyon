#include <Services/ContextService.h>

#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>
#include <Game/Player.h>
#include <World.h>

ContextService::ContextService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_playerJoinConnection(aDispatcher.sink<PlayerJoinEvent>().connect<&ContextService::OnPlayerJoin>(this))
    , m_playerLeaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&ContextService::OnPlayerLeave>(this))
{
}

void ContextService::OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept
{
    if (!m_enabled || !acEvent.pPlayer)
        return;

    EnsurePersonalContext(*acEvent.pPlayer);
}

void ContextService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    if (!acEvent.pPlayer)
        return;

    OnPlayerDisconnected(*acEvent.pPlayer);
}

std::optional<Halcyon::PlayerId> ContextService::FindPlayerId(const Player& acPlayer) const noexcept
{
    const auto it = m_playerIds.find(acPlayer.GetId());
    if (it == m_playerIds.end())
        return std::nullopt;

    return it->second;
}

Halcyon::ContextId ContextService::EnsurePersonalContext(Player& aPlayer) noexcept
{
    if (!m_enabled)
        return Halcyon::kInvalidContextId;

    auto playerId = FindPlayerId(aPlayer);
    if (!playerId)
    {
        playerId = m_nextPlayerId++;
        m_playerIds.emplace(aPlayer.GetId(), *playerId);
    }

    const auto existing = m_personalContexts.find(*playerId);
    if (existing != m_personalContexts.end())
    {
        // Re-joining an existing Context, e.g. after a reconnect within the
        // same server run.
        m_registry.AddMember(existing->second, *playerId);
        return existing->second;
    }

    const Halcyon::ContextId context = m_registry.CreateContext(Halcyon::ContextKind::Personal);
    m_registry.AddMember(context, *playerId);
    m_personalContexts.emplace(*playerId, context);

    spdlog::debug("Context {} created for player {:x}", context, aPlayer.GetId());

    return context;
}

void ContextService::OnPlayerDisconnected(const Player& acPlayer) noexcept
{
    // Deliberately does not erase the server-id mapping. Dropping it would make
    // a reconnecting Player allocate a fresh PlayerId, and therefore a second
    // Personal Context, orphaning the scoped state recorded before the
    // disconnect.
    //
    // Keeping it leaks one small entry per Player for the lifetime of the
    // process. That is acceptable for the prototype and disappears once a
    // persistent account identity replaces Player::GetId; see the note in
    // FindPlayerId.
    (void)acPlayer;
}

std::optional<Halcyon::ContextId> ContextService::GetPersonalContext(const Player& acPlayer) const noexcept
{
    const auto playerId = FindPlayerId(acPlayer);
    if (!playerId)
        return std::nullopt;

    const auto it = m_personalContexts.find(*playerId);
    if (it == m_personalContexts.end())
        return std::nullopt;

    return it->second;
}

bool ContextService::RecordLifeState(const Player& acPlayer, Halcyon::EntityId aEntity, bool aDead) noexcept
{
    if (!m_enabled)
        return false;

    const auto context = GetPersonalContext(acPlayer);
    if (!context)
    {
        spdlog::warn("Player {:x} has no personal context, dropping scoped life state", acPlayer.GetId());
        return false;
    }

    const auto result = m_registry.SetLifeState(*context, aEntity, aDead, m_nextRevision++);

    if (result != Halcyon::MutationResult::Applied)
    {
        spdlog::warn("Scoped life state rejected for entity {:x} in context {}", aEntity, *context);
        return false;
    }

    spdlog::debug("Entity {:x} dead={} in context {}", aEntity, aDead, *context);

    return true;
}

std::optional<bool> ContextService::GetObservedLifeState(const Player& acObserver, Halcyon::EntityId aEntity) const noexcept
{
    if (!m_enabled)
        return std::nullopt;

    const auto playerId = FindPlayerId(acObserver);
    if (!playerId)
        return std::nullopt;

    // Only Contexts the observer belongs to may contribute. The prototype has
    // at most one Personal Context per Player, so no precedence rule between
    // several matching Contexts is needed yet; HTDS-200 section 11 covers the
    // general case and is deliberately not implemented here.
    for (const Halcyon::ContextId context : m_registry.GetPlayerContexts(*playerId))
    {
        if (const auto state = m_registry.GetLifeState(context, aEntity))
            return state->dead;
    }

    return std::nullopt;
}
