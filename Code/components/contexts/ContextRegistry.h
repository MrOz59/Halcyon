#pragma once

// Halcyon Context system — prototype server-side registry.
//
// Status: Prototype. Server-side only: this type performs no replication,
// no persistence and no protocol work. It exists to validate the state model
// of RFC-0001 in isolation before any of those subsystems are touched.
//
// Not thread-safe. The prototype assumes it is driven from the server tick,
// matching how the existing services in Code/server are updated.

#include "Context.h"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace Halcyon
{
// Outcome of a scoped mutation. Callers must distinguish a rejected stale
// write from a successful one; AGENTS.md section 12 requires stale revisions
// to be rejected rather than silently applied.
enum class MutationResult
{
    Applied,
    RejectedStaleRevision,
    RejectedUnknownContext
};

class ContextRegistry
{
public:
    // Creates a Context and returns its server-issued id. Ids are allocated
    // sequentially from 1; kInvalidContextId is never returned.
    ContextId CreateContext(ContextKind aKind)
    {
        const ContextId id = m_nextContextId++;
        m_contexts.emplace(id, Context{id, aKind, 0});
        return id;
    }

    [[nodiscard]] bool HasContext(ContextId aContext) const { return m_contexts.find(aContext) != m_contexts.end(); }

    [[nodiscard]] std::optional<Context> GetContext(ContextId aContext) const
    {
        const auto it = m_contexts.find(aContext);
        if (it == m_contexts.end())
            return std::nullopt;

        return it->second;
    }

    // Membership. The prototype supports full membership only; roles and
    // guest/observer participation from HTDS-200 section 10 are out of scope.
    bool AddMember(ContextId aContext, PlayerId aPlayer)
    {
        if (!HasContext(aContext))
            return false;

        m_members[aContext].insert(aPlayer);
        m_playerContexts[aPlayer].insert(aContext);
        return true;
    }

    bool RemoveMember(ContextId aContext, PlayerId aPlayer)
    {
        const auto contextIt = m_members.find(aContext);
        if (contextIt == m_members.end() || contextIt->second.erase(aPlayer) == 0)
            return false;

        const auto playerIt = m_playerContexts.find(aPlayer);
        if (playerIt != m_playerContexts.end())
        {
            playerIt->second.erase(aContext);
            if (playerIt->second.empty())
                m_playerContexts.erase(playerIt);
        }

        if (contextIt->second.empty())
            m_members.erase(contextIt);

        return true;
    }

    [[nodiscard]] bool IsMember(ContextId aContext, PlayerId aPlayer) const
    {
        const auto it = m_members.find(aContext);
        return it != m_members.end() && it->second.find(aPlayer) != it->second.end();
    }

    // The active Context set of one Player, used by the replication rule in
    // RFC-0001 to decide who may observe a scoped value.
    [[nodiscard]] std::vector<ContextId> GetPlayerContexts(PlayerId aPlayer) const
    {
        const auto it = m_playerContexts.find(aPlayer);
        if (it == m_playerContexts.end())
            return {};

        return {it->second.begin(), it->second.end()};
    }

    [[nodiscard]] std::vector<PlayerId> GetMembers(ContextId aContext) const
    {
        const auto it = m_members.find(aContext);
        if (it == m_members.end())
            return {};

        return {it->second.begin(), it->second.end()};
    }

    // Records an Actor life state inside one Context. The write is rejected
    // when aRevision is not newer than the stored one, so that a replayed or
    // reordered mutation cannot resurrect an Actor.
    MutationResult SetLifeState(ContextId aContext, EntityId aEntity, bool aDead, Revision aRevision)
    {
        if (!HasContext(aContext))
            return MutationResult::RejectedUnknownContext;

        auto& state = m_lifeStates[Key{aContext, aEntity}];

        // A default-constructed entry has revision 0, which is why valid
        // revisions start at 1.
        if (state.revision >= aRevision)
            return MutationResult::RejectedStaleRevision;

        state = ScopedLifeState{aContext, aEntity, aDead, aRevision};
        m_contexts[aContext].revision = aRevision;

        return MutationResult::Applied;
    }

    // Returns the life state as seen from inside aContext, or nullopt when
    // that Context never recorded one. Callers treat nullopt as "unchanged
    // from base game data" rather than as "alive".
    [[nodiscard]] std::optional<ScopedLifeState> GetLifeState(ContextId aContext, EntityId aEntity) const
    {
        const auto it = m_lifeStates.find(Key{aContext, aEntity});
        if (it == m_lifeStates.end())
            return std::nullopt;

        return it->second;
    }

    // Implements the RFC-0001 replication rule: a scoped value is visible to a
    // Player only when the owning Context is in that Player's active set.
    [[nodiscard]] bool IsVisibleTo(const ScopedLifeState& acState, PlayerId aPlayer) const
    {
        return IsMember(acState.context, aPlayer);
    }

    // Every scoped value a Player is entitled to observe. This is the payload
    // a Context membership snapshot would carry once protocol work begins.
    [[nodiscard]] std::vector<ScopedLifeState> GetVisibleLifeStates(PlayerId aPlayer) const
    {
        std::vector<ScopedLifeState> visible;

        for (const auto& [key, state] : m_lifeStates)
        {
            if (IsMember(key.context, aPlayer))
                visible.push_back(state);
        }

        return visible;
    }

    [[nodiscard]] std::size_t GetContextCount() const { return m_contexts.size(); }

    // Recreates a Context with an id issued in an earlier run. Used only when
    // restoring persisted state; normal allocation goes through CreateContext.
    // Returns false when the id is invalid or already present, so a corrupt
    // snapshot cannot silently collide with live state.
    bool RestoreContext(ContextId aContext, ContextKind aKind, Revision aRevision)
    {
        if (aContext == kInvalidContextId || HasContext(aContext))
            return false;

        m_contexts.emplace(aContext, Context{aContext, aKind, aRevision});

        // Keep allocation ahead of every restored id.
        if (aContext >= m_nextContextId)
            m_nextContextId = aContext + 1;

        return true;
    }

    // Restores a scoped value without the newer-revision check, which only
    // applies to live mutations. Returns false for an unknown Context.
    bool RestoreLifeState(const ScopedLifeState& acState)
    {
        if (!HasContext(acState.context))
            return false;

        m_lifeStates[Key{acState.context, acState.entity}] = acState;
        return true;
    }

    // Every scoped value held, for snapshotting.
    [[nodiscard]] std::vector<ScopedLifeState> GetAllLifeStates() const
    {
        std::vector<ScopedLifeState> states;
        states.reserve(m_lifeStates.size());

        for (const auto& [key, state] : m_lifeStates)
            states.push_back(state);

        return states;
    }

    [[nodiscard]] std::vector<ContextId> GetAllContexts() const
    {
        std::vector<ContextId> contexts;
        contexts.reserve(m_contexts.size());

        for (const auto& [id, context] : m_contexts)
            contexts.push_back(id);

        return contexts;
    }

    [[nodiscard]] ContextId PeekNextContextId() const { return m_nextContextId; }

private:
    struct Key
    {
        ContextId context;
        EntityId entity;

        bool operator<(const Key& acRhs) const
        {
            if (context != acRhs.context)
                return context < acRhs.context;

            return entity < acRhs.entity;
        }
    };

    // Ordered containers keep iteration deterministic, which matters for
    // reproducible diagnostics while the prototype is being validated.
    ContextId m_nextContextId{1};
    std::map<ContextId, Context> m_contexts;
    std::map<ContextId, std::set<PlayerId>> m_members;
    std::map<PlayerId, std::set<ContextId>> m_playerContexts;
    std::map<Key, ScopedLifeState> m_lifeStates;
};
} // namespace Halcyon
