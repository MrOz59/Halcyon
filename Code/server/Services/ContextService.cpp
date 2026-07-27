#include <Services/ContextService.h>

#include <Events/PlayerJoinEvent.h>
#include <Events/PlayerLeaveEvent.h>
#include <Game/Player.h>
#include <World.h>

#include <cctype>

// Halcyon Context prototype (RFC-0001). Off by default: enabling it changes how
// Actor death is replicated, and the prototype is not validated. See
// docs/RFC/0001-context-system.md before turning this on.
Console::Setting bEnableContexts{"Halcyon:bEnableContexts", "(Prototype, unvalidated) Isolate quest-critical actor death per player Context", false};

ContextService::ContextService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_playerJoinConnection(aDispatcher.sink<PlayerJoinEvent>().connect<&ContextService::OnPlayerJoin>(this))
    , m_playerLeaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&ContextService::OnPlayerLeave>(this))
{
    // Settings are registered before this runs but are loaded from the ini
    // afterwards, so the value is picked up on first use rather than here.
}

void ContextService::SyncEnabledFromSettings() noexcept
{
    const bool enabled = bEnableContexts;

    if (enabled == m_enabled)
        return;

    m_enabled = enabled;

    if (!m_enabled)
    {
        spdlog::info("Halcyon Context prototype disabled");
        return;
    }

    spdlog::warn("Halcyon Context prototype ENABLED - actor death is scoped per player Context. This is an unvalidated prototype (RFC-0001).");

    // Restore whatever a previous run persisted, now that the prototype is
    // known to be on.
    Load();
}

void ContextService::OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept
{
    // First Player join is the earliest point where the ini has certainly been
    // applied, so the flag is resolved here rather than in the constructor.
    SyncEnabledFromSettings();

    if (!m_enabled || !acEvent.pPlayer)
        return;

    EnsurePersonalContext(*acEvent.pPlayer);
}

void ContextService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    if (!acEvent.pPlayer)
        return;

    OnPlayerDisconnected(*acEvent.pPlayer);

    // Persist on leave so scoped state survives an unclean shutdown that never
    // reaches a graceful save.
    if (m_enabled)
        Save();
}

std::string ContextService::MakeAccountKey(const Player& acPlayer) noexcept
{
    // The store format is whitespace-separated, so the key must not contain
    // any. Usernames are player-supplied and unvalidated, hence the scrubbing.
    std::string key(acPlayer.GetUsername().c_str());

    for (char& character : key)
    {
        if (std::isspace(static_cast<unsigned char>(character)))
            character = '_';
    }

    if (key.empty())
        key = "anonymous";

    return key;
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
        // Recognise a returning Player by account key before allocating a new
        // identity, so a restored Context is rejoined rather than duplicated.
        const std::string accountKey = MakeAccountKey(aPlayer);

        const auto known = m_accountKeys.find(accountKey);
        if (known != m_accountKeys.end())
        {
            playerId = known->second;
        }
        else
        {
            playerId = m_nextPlayerId++;
            m_accountKeys.emplace(accountKey, *playerId);
            m_playerAccountKeys.emplace(*playerId, accountKey);
        }

        m_playerIds.emplace(aPlayer.GetId(), *playerId);
    }

    const auto existing = m_personalContexts.find(*playerId);
    if (existing != m_personalContexts.end())
    {
        // Rejoining a Context from earlier in this run, or one restored from
        // the persisted snapshot.
        m_registry.AddMember(existing->second, *playerId);
        return existing->second;
    }

    const Halcyon::ContextId context = m_registry.CreateContext(Halcyon::ContextKind::Personal);
    m_registry.AddMember(context, *playerId);
    m_personalContexts.emplace(*playerId, context);

    spdlog::debug("Context {} created for player {:x}", context, aPlayer.GetId());

    return context;
}

bool ContextService::Save() noexcept
{
    if (!m_enabled)
        return false;

    Halcyon::ContextSnapshot snapshot;
    snapshot.nextContextId = m_registry.PeekNextContextId();
    snapshot.nextRevision = m_nextRevision;
    snapshot.lifeStates = m_registry.GetAllLifeStates();

    for (const auto& [playerId, context] : m_personalContexts)
    {
        const auto keyIt = m_playerAccountKeys.find(playerId);
        if (keyIt == m_playerAccountKeys.end())
            continue;

        Halcyon::PersistentMembership membership;
        membership.accountKey = keyIt->second;
        membership.context = context;
        membership.kind = Halcyon::ContextKind::Personal;
        snapshot.memberships.push_back(membership);
    }

    if (!Halcyon::ContextStore::SaveToFile(snapshot, m_storePath))
    {
        spdlog::error("Failed to save context store to {}", m_storePath);
        return false;
    }

    spdlog::debug("Saved {} contexts and {} scoped states", snapshot.memberships.size(), snapshot.lifeStates.size());

    return true;
}

bool ContextService::Load() noexcept
{
    if (!m_enabled)
        return false;

    if (!Halcyon::ContextStore::FileExists(m_storePath))
    {
        // A first run is not a failure.
        return false;
    }

    Halcyon::ContextSnapshot snapshot;
    if (!Halcyon::ContextStore::LoadFromFile(m_storePath, snapshot))
    {
        // Refuse to start from a file that cannot be parsed rather than
        // silently continuing with empty state and overwriting it on the next
        // save.
        spdlog::error("Context store at {} is unreadable or malformed; scoped state was NOT loaded", m_storePath);
        return false;
    }

    for (const auto& membership : snapshot.memberships)
    {
        const Halcyon::PlayerId playerId = m_nextPlayerId++;

        m_accountKeys.emplace(membership.accountKey, playerId);
        m_playerAccountKeys.emplace(playerId, membership.accountKey);

        if (!m_registry.RestoreContext(membership.context, membership.kind, 0))
        {
            spdlog::warn("Duplicate context {} in store, skipping", membership.context);
            continue;
        }

        m_registry.AddMember(membership.context, playerId);
        m_personalContexts.emplace(playerId, membership.context);
    }

    for (const auto& state : snapshot.lifeStates)
    {
        if (!m_registry.RestoreLifeState(state))
            spdlog::warn("Scoped state for unknown context {} in store, skipping", state.context);
    }

    m_nextRevision = snapshot.nextRevision;

    spdlog::info("Loaded {} contexts and {} scoped states from {}", snapshot.memberships.size(), snapshot.lifeStates.size(), m_storePath);

    return true;
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
