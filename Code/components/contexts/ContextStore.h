#pragma once

// Halcyon Context system — prototype persistence.
//
// Status: Prototype. Serializes the Context registry to a plain text file so
// that scoped state survives a server restart, which is what RFC-0001 steps
// 9-10 require.
//
// This is NOT the persistence architecture described in HTDS-170. There is no
// database, no transaction boundary, no schema migration, no audit trail and no
// crash-safe commit beyond a write-to-temp-and-rename. It exists so the first
// prototype can be validated end to end; a durable design replaces it.
//
// See docs/RFC/0001-context-system.md and docs/HTDS/Volume-2-Core-Architecture/
// 170-Persistence.md.

#include "ContextRegistry.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace Halcyon
{
// One durable Context membership entry. AccountKey is the identity a returning
// Player is recognised by; see ContextStore's note on how weak that is today.
struct PersistentMembership
{
    std::string accountKey;
    ContextId context{kInvalidContextId};
    ContextKind kind{ContextKind::Personal};
};

struct ContextSnapshot
{
    // Highest Context id ever issued, so ids are not reused after a restart.
    ContextId nextContextId{1};
    Revision nextRevision{1};

    std::vector<PersistentMembership> memberships;
    std::vector<ScopedLifeState> lifeStates;
};

// Line-oriented text format, versioned so a future reader can reject or migrate
// what it does not understand.
//
//   halcyon-context-store <version>
//   next <nextContextId> <nextRevision>
//   member <accountKey> <contextId> <kind>
//   life <contextId> <entityId> <dead> <revision>
//
// accountKey is written last on its line only in the sense that it must not
// contain whitespace; callers are responsible for that and Serialize rejects
// keys that violate it rather than writing a file it cannot read back.
class ContextStore
{
public:
    // Version 2 changed what EntityId means: version 1 wrote per-session entt
    // handles, which resolve to nothing after a restart. A v1 file would load
    // without error and silently reference entities that no longer exist, so
    // the version gate rejects it and the server starts from empty state.
    static constexpr int kFormatVersion = 2;

    // Returns false when the snapshot cannot be represented, leaving aOutput
    // untouched. The only current cause is an account key containing
    // whitespace, which would silently corrupt the line format.
    static bool Serialize(const ContextSnapshot& acSnapshot, std::string& aOutput)
    {
        for (const auto& membership : acSnapshot.memberships)
        {
            if (membership.accountKey.empty() || HasWhitespace(membership.accountKey))
                return false;
        }

        std::ostringstream stream;
        stream << "halcyon-context-store " << kFormatVersion << '\n';
        stream << "next " << acSnapshot.nextContextId << ' ' << acSnapshot.nextRevision << '\n';

        for (const auto& membership : acSnapshot.memberships)
        {
            stream << "member " << membership.accountKey << ' ' << membership.context << ' '
                   << static_cast<std::uint32_t>(membership.kind) << '\n';
        }

        for (const auto& state : acSnapshot.lifeStates)
        {
            stream << "life " << state.context << ' ' << state.entity << ' ' << (state.dead ? 1 : 0) << ' '
                   << state.revision << '\n';
        }

        aOutput = stream.str();
        return true;
    }

    // Returns false on a malformed or unsupported file, leaving aSnapshot
    // untouched. A partially parsed file is never applied: a truncated write
    // must not silently drop half the scoped state.
    static bool Deserialize(const std::string& acInput, ContextSnapshot& aSnapshot)
    {
        std::istringstream stream(acInput);
        std::string line;

        if (!std::getline(stream, line))
            return false;

        {
            std::istringstream header(line);
            std::string magic;
            int version = 0;
            if (!(header >> magic >> version) || magic != "halcyon-context-store" || version != kFormatVersion)
                return false;
        }

        ContextSnapshot parsed;
        bool sawNext = false;

        while (std::getline(stream, line))
        {
            if (line.empty())
                continue;

            std::istringstream record(line);
            std::string kind;
            if (!(record >> kind))
                return false;

            if (kind == "next")
            {
                if (!(record >> parsed.nextContextId >> parsed.nextRevision))
                    return false;

                sawNext = true;
            }
            else if (kind == "member")
            {
                PersistentMembership membership;
                std::uint32_t rawKind = 0;
                if (!(record >> membership.accountKey >> membership.context >> rawKind))
                    return false;

                if (membership.context == kInvalidContextId || rawKind > static_cast<std::uint32_t>(ContextKind::Instance))
                    return false;

                membership.kind = static_cast<ContextKind>(rawKind);
                parsed.memberships.push_back(membership);
            }
            else if (kind == "life")
            {
                ScopedLifeState state;
                int dead = 0;
                if (!(record >> state.context >> state.entity >> dead >> state.revision))
                    return false;

                if (state.context == kInvalidContextId || (dead != 0 && dead != 1))
                    return false;

                state.dead = dead == 1;
                parsed.lifeStates.push_back(state);
            }
            else
            {
                // Unknown record: reject rather than skip, so a newer file is
                // never half-applied by an older server.
                return false;
            }
        }

        if (!sawNext)
            return false;

        // Ids must not be reissued over persisted state.
        for (const auto& membership : parsed.memberships)
        {
            if (membership.context >= parsed.nextContextId)
                return false;
        }

        for (const auto& state : parsed.lifeStates)
        {
            if (state.context >= parsed.nextContextId || state.revision >= parsed.nextRevision)
                return false;
        }

        aSnapshot = std::move(parsed);
        return true;
    }

    // Writes to a temporary file and renames over the target, so an interrupted
    // write leaves the previous snapshot intact rather than a truncated one.
    // This is not a durability guarantee: no fsync is issued, so a host crash
    // can still lose the most recent write.
    static bool SaveToFile(const ContextSnapshot& acSnapshot, const std::string& acPath)
    {
        std::string payload;
        if (!Serialize(acSnapshot, payload))
            return false;

        const std::string temporaryPath = acPath + ".tmp";

        {
            std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;

            file << payload;
            if (!file)
                return false;
        }

        std::error_code ec;
        std::filesystem::rename(temporaryPath, acPath, ec);
        if (ec)
        {
            std::filesystem::remove(temporaryPath, ec);
            return false;
        }

        return true;
    }

    // Returns false when the file is absent or malformed. An absent file is a
    // normal first run and is not an error the caller must treat as failure,
    // so callers should distinguish the two via FileExists when it matters.
    static bool LoadFromFile(const std::string& acPath, ContextSnapshot& aSnapshot)
    {
        std::ifstream file(acPath, std::ios::binary);
        if (!file)
            return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (!file && !file.eof())
            return false;

        return Deserialize(buffer.str(), aSnapshot);
    }

    static bool FileExists(const std::string& acPath)
    {
        std::error_code ec;
        return std::filesystem::exists(acPath, ec) && !ec;
    }

private:
    static bool HasWhitespace(const std::string& acValue)
    {
        return acValue.find_first_of(" \t\r\n\v\f") != std::string::npos;
    }
};
} // namespace Halcyon
