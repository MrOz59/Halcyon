#include <Services/CombatService.h>
#include <Services/TransportService.h>

#include <Events/UpdateEvent.h>
#include <Events/ProjectileLaunchedEvent.h>
#include <Events/HitEvent.h>

#include <Messages/ProjectileLaunchRequest.h>
#include <Messages/NotifyProjectileLaunch.h>

#include <Systems/ModSystem.h>
#include <World.h>

#include <Projectiles/Projectile.h>
#include <Forms/TESObjectWEAP.h>
#include <Forms/TESAmmo.h>
#include <Games/ActorExtension.h>
#include <Games/References.h>
#include <Games/Skyrim/Interface/TrueHUDAPI.h>

#include <algorithm>

CombatService::CombatService(World& aWorld, TransportService& aTransport, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&CombatService::OnUpdate>(this);
    m_hitConnection = aDispatcher.sink<HitEvent>().connect<&CombatService::OnHitEvent>(this);
    m_localComponentRemoved = m_world.on_destroy<LocalComponent>().connect<&CombatService::OnLocalComponentRemoved>(this);
    m_projectileLaunchedConnection = aDispatcher.sink<ProjectileLaunchedEvent>().connect<&CombatService::OnProjectileLaunchedEvent>(this);
    m_projectileLaunchConnection = aDispatcher.sink<NotifyProjectileLaunch>().connect<&CombatService::OnNotifyProjectileLaunch>(this);
}

void CombatService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    RunTargetUpdates(static_cast<float>(acEvent.Delta));
    RunPvpBarUpdates();
}

void CombatService::OnLocalComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) const noexcept
{
    m_world.remove<CombatComponent>(aEntity);
}

void CombatService::OnProjectileLaunchedEvent(const ProjectileLaunchedEvent& acEvent) const noexcept
{
    ModSystem& modSystem = m_world.Get().GetModSystem();

    uint32_t shooterFormId = acEvent.ShooterID;
    auto view = m_world.view<FormIdComponent, LocalComponent>();
    const auto shooterEntityIt = std::find_if(std::begin(view), std::end(view), [shooterFormId, view](entt::entity entity) { return view.get<FormIdComponent>(entity).Id == shooterFormId; });

    if (shooterEntityIt == std::end(view))
        return;

    LocalComponent& localComponent = view.get<LocalComponent>(*shooterEntityIt);

    ProjectileLaunchRequest request{};

    request.OriginX = acEvent.Origin.x;
    request.OriginY = acEvent.Origin.y;
    request.OriginZ = acEvent.Origin.z;

    modSystem.GetServerModId(acEvent.ProjectileBaseID, request.ProjectileBaseID);
    modSystem.GetServerModId(acEvent.WeaponID, request.WeaponID);
    modSystem.GetServerModId(acEvent.AmmoID, request.AmmoID);

    request.ShooterID = localComponent.Id;

    request.ZAngle = acEvent.ZAngle;
    request.XAngle = acEvent.XAngle;
    request.YAngle = acEvent.YAngle;

    modSystem.GetServerModId(acEvent.ParentCellID, request.ParentCellID);
    modSystem.GetServerModId(acEvent.SpellID, request.SpellID);

    request.CastingSource = acEvent.CastingSource;

    request.Area = acEvent.Area;
    request.Power = acEvent.Power;
    request.Scale = acEvent.Scale;

    request.AlwaysHit = acEvent.AlwaysHit;
    request.NoDamageOutsideCombat = acEvent.NoDamageOutsideCombat;
    request.AutoAim = acEvent.AutoAim;
    request.DeferInitialization = acEvent.DeferInitialization;
    request.ForceConeOfFire = acEvent.ForceConeOfFire;

    request.UnkBool1 = acEvent.UnkBool1;
    request.UnkBool2 = acEvent.UnkBool2;

    m_transport.Send(request);
}

void CombatService::OnNotifyProjectileLaunch(const NotifyProjectileLaunch& acMessage) const noexcept
{
    ModSystem& modSystem = World::Get().GetModSystem();

    auto remoteView = m_world.view<RemoteComponent, FormIdComponent>();
    const auto remoteIt = std::find_if(std::begin(remoteView), std::end(remoteView), [remoteView, Id = acMessage.ShooterID](auto entity) { return remoteView.get<RemoteComponent>(entity).Id == Id; });

    if (remoteIt == std::end(remoteView))
    {
        spdlog::warn("Shooter with remote id {:X} not found.", acMessage.ShooterID);
        return;
    }

    FormIdComponent formIdComponent = remoteView.get<FormIdComponent>(*remoteIt);

    Projectile::LaunchData launchData{};

    // Projectile::Launch relies on pParentCell being valid.
    // TODO: it's possible that more of these values must have a value, should probably check for that in the game code.
    const uint32_t cParentCellId = modSystem.GetGameId(acMessage.ParentCellID);
    launchData.pParentCell = Cast<TESObjectCELL>(TESForm::GetById(cParentCellId));

    if (!launchData.pParentCell)
    {
        spdlog::warn("Cannot launch projectile, invalid parent cell: {:X}", cParentCellId);
        return;
    }

    launchData.pShooter = Cast<TESObjectREFR>(TESForm::GetById(formIdComponent.Id));

    launchData.Origin.x = acMessage.OriginX;
    launchData.Origin.y = acMessage.OriginY;
    launchData.Origin.z = acMessage.OriginZ;

    const uint32_t cProjectileBaseId = modSystem.GetGameId(acMessage.ProjectileBaseID);
    launchData.pProjectileBase = TESForm::GetById(cProjectileBaseId);

    const uint32_t cFromWeaponId = modSystem.GetGameId(acMessage.WeaponID);
    launchData.pFromWeapon = Cast<TESObjectWEAP>(TESForm::GetById(cFromWeaponId));

    const uint32_t cFromAmmoId = modSystem.GetGameId(acMessage.AmmoID);
    launchData.pFromAmmo = Cast<TESAmmo>(TESForm::GetById(cFromAmmoId));

    launchData.fZAngle = acMessage.ZAngle;
    launchData.fXAngle = acMessage.XAngle;
    launchData.fYAngle = acMessage.YAngle;

    const uint32_t cSpellId = modSystem.GetGameId(acMessage.SpellID);
    launchData.pSpell = Cast<MagicItem>(TESForm::GetById(cSpellId));

    launchData.eCastingSource = (MagicSystem::CastingSource)acMessage.CastingSource;

    launchData.iArea = acMessage.Area;
    launchData.fPower = acMessage.Power;
    launchData.fScale = acMessage.Scale;

    launchData.bAlwaysHit = acMessage.AlwaysHit;
    launchData.bNoDamageOutsideCombat = acMessage.NoDamageOutsideCombat;
    launchData.bAutoAim = acMessage.AutoAim;

    launchData.bForceConeOfFire = acMessage.ForceConeOfFire;

    // always use origin, or it'll recalculate it and it desyncs
    launchData.bUseOrigin = true;

    launchData.bUnkBool1 = acMessage.UnkBool1;
    launchData.bUnkBool2 = acMessage.UnkBool2;

    BSPointerHandle<Projectile> result;

    Projectile::Launch(&result, launchData);
}

void CombatService::OnHitEvent(const HitEvent& acEvent) noexcept
{
    // Show a health bar for the player being fought. Only while PvP is on:
    // with PvP off players cannot hurt each other, so a bar would be noise.
    if (m_transport.IsConnected() && World::Get().GetServerSettings().PvpEnabled)
    {
        auto* pHitter = Cast<Actor>(TESForm::GetById(acEvent.HitterId));
        auto* pHittee = Cast<Actor>(TESForm::GetById(acEvent.HitteeId));

        if (pHitter && pHittee)
        {
            // Only pairs involving us: the local HUD can only show the bar of
            // the other side of our own fight. HookDamageActor runs on both
            // clients, so each side ends up showing the other's bar.
            if (pHitter->GetExtension()->IsLocalPlayer() && pHittee->GetExtension()->IsRemotePlayer())
                BeginPvpBar(pHittee);
            else if (pHitter->GetExtension()->IsRemotePlayer() && pHittee->GetExtension()->IsLocalPlayer())
                BeginPvpBar(pHitter);
        }
    }

#if 0
    if (!m_transport.IsConnected())
        return;

    // The targeting system does not apply to players, and only local actors should be considered.
    auto* pHittee = Cast<Actor>(TESForm::GetById(acEvent.HitteeId));
    if (!pHittee || pHittee->GetExtension()->IsPlayer() || pHittee->GetExtension()->IsRemote())
        return;

    // NPCs should not affect targeting.
    auto* pHitter = Cast<Actor>(TESForm::GetById(acEvent.HitterId));
    if (!pHitter || !pHitter->GetExtension()->IsPlayer())
        return;

    // If the target is not in combat, don't start combat, let the game take care of that first.
    if (!pHittee->IsInCombat())
        return;

    auto view = m_world.view<FormIdComponent, LocalComponent>(entt::exclude<ObjectComponent>);

    const auto hitteeIt = std::find_if(std::begin(view), std::end(view), [id = acEvent.HitteeId, view](entt::entity entity) { return view.get<FormIdComponent>(entity).Id == id; });

    if (hitteeIt == std::end(view))
    {
        spdlog::warn(__FUNCTION__ ": hittee form id component not found, form id: {:X}", acEvent.HitterId);
        return;
    }

    if (m_world.any_of<CombatComponent>(*hitteeIt))
        return;

    m_world.emplace_or_replace<CombatComponent>(*hitteeIt, acEvent.HitterId);

    pHittee->SetCombatTargetEx(pHitter);
#endif
}

void CombatService::RunTargetUpdates(const float acDelta) const noexcept
{
#if 0
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 200ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    const auto view = m_world.view<FormIdComponent, CombatComponent>();

    Vector<entt::entity> toRemove{};

    for (const auto entity : view)
    {
        auto& combatComponent = view.get<CombatComponent>(entity);
        combatComponent.Timer = combatComponent.Timer - acDelta;

        if (combatComponent.Timer <= 0.f)
        {
            toRemove.push_back(entity);
            continue;
        }

        auto* pTarget = Cast<Actor>(TESForm::GetById(combatComponent.TargetFormId));
        if (!pTarget)
        {
            spdlog::warn(__FUNCTION__ ": combat target not found, form id {:X}", combatComponent.TargetFormId);
            toRemove.push_back(entity);
            continue;
        }

        const auto& formIdComponent = view.get<FormIdComponent>(entity);
        auto* pActor = Cast<Actor>(TESForm::GetById(formIdComponent.Id));
        if (!pActor)
        {
            spdlog::error(__FUNCTION__ ": actor not found, form id {:X}", formIdComponent.Id);
            toRemove.push_back(entity);
            continue;
        }
    }

    for (const auto entity : toRemove)
        m_world.remove<CombatComponent>(entity);
#endif
}

void CombatService::BeginPvpBar(Actor* apRemote) noexcept
{
    auto* pTrueHud = TRUEHUD_API::RequestTrueHUDInterface();
    if (!pTrueHud)
        return;

    // Without the target slot a bar would be created and then hidden again, so
    // there is nothing to gain from asking.
    if (!TRUEHUD_API::HasTargetControl())
        return;

    const auto cHandle = apRemote->GetHandle();

    // TrueHUD drops a request whose handle is zero or the engine's null handle.
    if (!cHandle.handle.iBits || cHandle.handle.iBits == 0x100000)
    {
        spdlog::warn("[truehud] remote player {:X} has no usable handle ({:X}) - no bar", apRemote->formID, cHandle.handle.iBits);
        return;
    }

    const bool cIsNew = m_pvpEngagements.find(apRemote->formID) == m_pvpEngagements.end();

    auto& engagement = m_pvpEngagements[apRemote->formID];
    engagement.lastDamage = std::chrono::steady_clock::now();
    engagement.remoteHandle = cHandle;

    // Marking the player as TrueHUD's target is what actually makes the bar
    // visible. Every other category is additionally gated on the actor being in
    // combat (InfoBarBase: `targetType > kTarget && !IsInCombat()` hides the
    // bar), and a remote player never is - the game refuses to start combat
    // between actors that share the player faction. The target category has no
    // such condition.
    pTrueHud->SetTarget(TRUEHUD_API::kSkyrimTogetherPluginHandle, cHandle);

    // Asked on every hit, not just the first. TrueHUD drops a widget whose actor
    // was not ready when it was created ("isReadyToRemove"), which is what made
    // the bar miss the opening hit on the attacker's side while showing up
    // immediately for the side taking the damage. Re-requesting is cheap and
    // safe: TrueHUDMenu::AddActorInfoBar checks HasActorInfoBar first, so an
    // existing bar is left alone rather than duplicated.
    pTrueHud->AddActorInfoBar(cHandle);

    if (cIsNew)
        spdlog::info("[truehud] bar + target set for remote player {:X} (handle {:X})", apRemote->formID, cHandle.handle.iBits);
}

void CombatService::RunPvpBarUpdates() noexcept
{
    if (m_pvpEngagements.empty())
        return;

    auto* pTrueHud = TRUEHUD_API::RequestTrueHUDInterface();
    if (!pTrueHud)
    {
        m_pvpEngagements.clear();
        return;
    }

    // No damage traded for this long ends the fight.
    constexpr auto cPvpTimeout = 60s;

    auto* pLocalPlayer = PlayerCharacter::Get();
    if (!pLocalPlayer)
        return;

    const auto cNow = std::chrono::steady_clock::now();
    const bool cLocalSheathed = !pLocalPlayer->actorState.IsWeaponDrawn();

    // Map's iterators expose the value as const, so decide first and apply the
    // changes afterwards through operator[] and erase.
    Vector<uint32_t> ended;
    Vector<uint32_t> sawWeapons;
    Vector<uint32_t> reasserted;

    for (const auto& [remoteFormId, engagement] : m_pvpEngagements)
    {
        auto* pRemote = Cast<Actor>(TESForm::GetById(remoteFormId));

        // Gone, no longer a remote player, or dead.
        bool isOver = !pRemote || !pRemote->GetExtension()->IsRemotePlayer();

        if (!isOver)
            isOver = pRemote->IsDead() || pLocalPlayer->IsDead();

        if (!isOver)
            isOver = cNow - engagement.lastDamage >= cPvpTimeout;

        const bool cRemoteDrawn = pRemote && pRemote->actorState.IsWeaponDrawn();

        // Sheathing only counts once weapons have actually been seen out, so an
        // unarmed or spell fight is not ended on its first frame.
        if (!isOver && engagement.sawWeaponsDrawn)
            isOver = cLocalSheathed && !cRemoteDrawn;

        if (!isOver)
        {
            // A widget whose actor was not ready yet is dropped by TrueHUD, which
            // is what made the bar miss the opening hit on the attacker's side.
            // Re-assert it for a short while after each hit instead of only once,
            // so a single hit still ends up with a bar.
            //
            // Bounded on purpose: HUDHandler::AddActorInfoBar always queues a HUD
            // task, even when the bar already exists, so doing this every frame
            // for a whole fight would just pile work up.
            constexpr auto cReassertWindow = 2s;
            constexpr auto cReassertInterval = 250ms;
            if (cNow - engagement.lastDamage < cReassertWindow && cNow - engagement.lastReassert >= cReassertInterval)
            {
                pTrueHud->SetTarget(TRUEHUD_API::kSkyrimTogetherPluginHandle, engagement.remoteHandle);
                pTrueHud->AddActorInfoBar(engagement.remoteHandle);
                reasserted.push_back(remoteFormId);
            }

            // Recorded only for a running fight: operator[] would otherwise
            // resurrect an entry that is about to be erased.
            if ((!cLocalSheathed || cRemoteDrawn) && !engagement.sawWeaponsDrawn)
                sawWeapons.push_back(remoteFormId);
            continue;
        }

        // Delayed matches what TrueHUD does for dead targets: the bar lingers
        // briefly instead of vanishing mid-frame.
        pTrueHud->RemoveActorInfoBar(engagement.remoteHandle, TRUEHUD_API::WidgetRemovalMode::Delayed);
        ended.push_back(remoteFormId);
    }

    for (const uint32_t cRemoteFormId : sawWeapons)
        m_pvpEngagements[cRemoteFormId].sawWeaponsDrawn = true;

    for (const uint32_t cRemoteFormId : reasserted)
        m_pvpEngagements[cRemoteFormId].lastReassert = cNow;

    for (const uint32_t cRemoteFormId : ended)
        m_pvpEngagements.erase(cRemoteFormId);

    // Release the target once no fight is left, so a finished opponent does not
    // stay marked as our target for the rest of the session.
    if (m_pvpEngagements.empty() && !ended.empty())
        pTrueHud->SetTarget(TRUEHUD_API::kSkyrimTogetherPluginHandle, {});
}
