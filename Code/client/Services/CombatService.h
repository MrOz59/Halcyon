#pragma once

#include <chrono>

struct Actor;
struct TESObjectREFR;
struct World;
struct TransportService;
struct UpdateEvent;
struct ProjectileLaunchedEvent;
struct NotifyProjectileLaunch;
struct HitEvent;

/**
 * @brief Responsible for projectiles, combat agro, etc.
 */
struct CombatService
{
    CombatService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher);
    ~CombatService() noexcept = default;

    TP_NOCOPYMOVE(CombatService);

protected:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnLocalComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) const noexcept;
    void OnProjectileLaunchedEvent(const ProjectileLaunchedEvent& acEvent) const noexcept;
    void OnNotifyProjectileLaunch(const NotifyProjectileLaunch& acMessage) const noexcept;
    void OnHitEvent(const HitEvent& acEvent) noexcept;

    void RunTargetUpdates(const float acDelta) const noexcept;

    /**
     * @brief Show a health bar above a remote player being fought.
     *
     * The vanilla HUD only draws a bar for an actor the game treats as an
     * opponent, and remote players share the player faction, so it never
     * qualifies. TrueHUD can be told to show a bar for a specific actor, which
     * sidesteps faction and combat state entirely. Does nothing when TrueHUD is
     * not installed.
     */
    void BeginPvpBar(Actor* apRemote) noexcept;

    /**
     * @brief Drop the bars of finished fights.
     *
     * A fight ends when either side dies, when no damage is traded for a while,
     * or when both have sheathed their weapons.
     */
    void RunPvpBarUpdates() noexcept;

private:
    World& m_world;
    TransportService& m_transport;

    struct PvpEngagement
    {
        std::chrono::steady_clock::time_point lastDamage;
        // Throttles re-asserting the bar: TrueHUD queues a HUD task on every
        // request, so this must not run once per frame.
        std::chrono::steady_clock::time_point lastReassert;
        // Kept so the bar can still be removed after the actor is gone.
        BSPointerHandle<TESObjectREFR> remoteHandle{};
        // "Both sheathed" may only end a fight that was fought with weapons out,
        // otherwise a fist or spell exchange would end on its first frame.
        bool sawWeaponsDrawn = false;
    };

    // Keyed by the remote player's form id.
    Map<uint32_t, PvpEngagement> m_pvpEngagements;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_localComponentRemoved;
    entt::scoped_connection m_projectileLaunchedConnection;
    entt::scoped_connection m_projectileLaunchConnection;
    entt::scoped_connection m_hitConnection;
};
