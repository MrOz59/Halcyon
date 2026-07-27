#include <Components.h>
#include <Messages/RequestActorValueChanges.h>
#include <Messages/RequestActorMaxValueChanges.h>
#include <Messages/RequestHealthChangeBroadcast.h>
#include <Messages/RequestDeathStateChange.h>
#include <Services/ActorValueService.h>
#include <World.h>
#include <GameServer.h>
#include <Messages/NotifyActorValueChanges.h>
#include <Messages/NotifyActorMaxValueChanges.h>
#include <Messages/NotifyHealthChangeBroadcast.h>
#include <Messages/NotifyDeathStateChange.h>

ActorValueService::ActorValueService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_updateHealthConnection = aDispatcher.sink<PacketEvent<RequestActorValueChanges>>().connect<&ActorValueService::OnActorValueChanges>(this);
    m_updateMaxValueConnection = aDispatcher.sink<PacketEvent<RequestActorMaxValueChanges>>().connect<&ActorValueService::OnActorMaxValueChanges>(this);
    m_updateDeltaHealthConnection = aDispatcher.sink<PacketEvent<RequestHealthChangeBroadcast>>().connect<&ActorValueService::OnHealthChangeBroadcast>(this);
    m_deathStateConnection = aDispatcher.sink<PacketEvent<RequestDeathStateChange>>().connect<&ActorValueService::OnDeathStateChange>(this);
}

void ActorValueService::OnActorValueChanges(const PacketEvent<RequestActorValueChanges>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    if (it != actorValuesView.end())
    {
        auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
        for (auto& [id, value] : message.Values)
        {
            actorValuesComponent.CurrentActorValues.ActorValuesList[id] = value;
        }
    }

    NotifyActorValueChanges notify;
    notify.Id = acMessage.Packet.Id;
    notify.Values = acMessage.Packet.Values;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnActorMaxValueChanges(const PacketEvent<RequestActorMaxValueChanges>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    if (it != actorValuesView.end())
    {
        auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
        for (auto& [id, value] : message.Values)
        {
            actorValuesComponent.CurrentActorValues.ActorMaxValuesList[id] = value;
        }
    }

    NotifyActorMaxValueChanges notify;
    notify.Id = message.Id;
    notify.Values = message.Values;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnHealthChangeBroadcast(const PacketEvent<RequestHealthChangeBroadcast>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    // TODO(cosideci): should server side health not be updated?
    auto actorValuesView = m_world.view<ActorValuesComponent, OwnerComponent>();

    auto it = actorValuesView.find(static_cast<entt::entity>(message.Id));

    if (it != actorValuesView.end())
    {
        auto& actorValuesComponent = actorValuesView.get<ActorValuesComponent>(*it);
        auto currentHealth = actorValuesComponent.CurrentActorValues.ActorValuesList[24];
        actorValuesComponent.CurrentActorValues.ActorValuesList[24] = currentHealth - message.DeltaHealth;
    }

    NotifyHealthChangeBroadcast notify;
    notify.Id = message.Id;
    notify.DeltaHealth = message.DeltaHealth;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);
    if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
}

void ActorValueService::OnDeathStateChange(const PacketEvent<RequestDeathStateChange>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    auto characterView = m_world.view<CharacterComponent, OwnerComponent>();

    const auto it = characterView.find(static_cast<entt::entity>(message.Id));

    auto& contextService = m_world.GetContextService();

    // Halcyon Context prototype (RFC-0001). While the prototype is disabled
    // this is inert and the legacy global path below runs unchanged.
    //
    // message.Id is an entt handle, which is reallocated every run; scoping it
    // directly would persist state that resolves to nothing after a restart.
    // Fall through to the legacy path when no stable identity exists.
    Halcyon::EntityId scopedEntityId = 0;
    bool scoped = false;
    if (contextService.IsEnabled() && acMessage.pPlayer)
    {
        if (const auto entityId = contextService.ResolveEntityId(static_cast<entt::entity>(message.Id)))
        {
            scoped = contextService.RecordLifeState(*acMessage.pPlayer, *entityId, message.IsDead);
            scopedEntityId = *entityId;
        }
        else
        {
            spdlog::debug("No stable identity for entity {:x}, not scoping its death", message.Id);
        }
    }

    if (it != characterView.end() && !scoped)
    {
        // CharacterComponent holds a single life state per Entity, so it can
        // only represent the global view. A scoped death is deliberately not
        // written here: doing so would leak one Player's Context into every
        // other Player's world state.
        auto& characterComponent = characterView.get<CharacterComponent>(*it);
        characterComponent.SetDead(message.IsDead);
        spdlog::debug("Updating death state {:x}:{}", message.Id, message.IsDead);
    }

    NotifyDeathStateChange notify;
    notify.Id = message.Id;
    notify.IsDead = message.IsDead;

    const entt::entity cEntity = static_cast<entt::entity>(message.Id);

    if (scoped)
    {
        if (!GameServer::Get()->SendToPlayersInRangeObserving(notify, cEntity, scopedEntityId, message.IsDead, acMessage.pPlayer))
            spdlog::error("{}: SendToPlayersInRangeObserving failed", __FUNCTION__);

        // The acting Player is excluded from the broadcast above but owns the
        // scoped state, so tell them explicitly.
        acMessage.pPlayer->Send(notify);
    }
    else if (!GameServer::Get()->SendToPlayersInRange(notify, cEntity, acMessage.pPlayer))
    {
        spdlog::error("{}: SendToPlayersInRange failed", __FUNCTION__);
    }
}
