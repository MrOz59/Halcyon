#pragma once

// Halcyon Context system — stable entity identity.
//
// Status: Prototype. Derives a persistence-safe EntityId from a game-side
// record identity.
//
// The server addresses live entities by entt handle, which is reallocated on
// every run: scoped state keyed by a handle loads successfully after a restart
// and then matches nothing, leaving previously killed actors alive. The numeric
// ModId is no better -- ModsComponent assigns it from a counter in
// player-connection order.
//
// What survives a restart is the owning plugin's filename together with the
// record's BaseId, which is what this file hashes.
//
// See docs/RFC/0001-context-system.md.

#include "Context.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Halcyon
{
namespace detail
{
// FNV-1a, 64-bit. Chosen for being short, dependency-free and stable across
// platforms and compiler versions -- the value is written to disk, so it must
// not change between builds. It is not a cryptographic hash and is not used as
// one.
inline constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] constexpr std::uint64_t Fnv1a(std::string_view acText, std::uint64_t aSeed = kFnvOffsetBasis) noexcept
{
    std::uint64_t hash = aSeed;

    for (const char character : acText)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= kFnvPrime;
    }

    return hash;
}

[[nodiscard]] constexpr std::uint64_t Fnv1a(std::uint32_t aValue, std::uint64_t aSeed) noexcept
{
    std::uint64_t hash = aSeed;

    for (int shift = 0; shift < 32; shift += 8)
    {
        hash ^= static_cast<std::uint64_t>((aValue >> shift) & 0xFFu);
        hash *= kFnvPrime;
    }

    return hash;
}
} // namespace detail

// Builds a stable EntityId from the plugin that owns a record and the record's
// BaseId within that plugin.
//
// acPluginName must be the plugin filename as the server knows it (for example
// "Skyrim.esm"). Comparison is case-sensitive: callers that may see differing
// case must normalise before calling, because Windows and Linux clients can
// report the same plugin differently.
//
// Returns 0 when acPluginName is empty, which callers must treat as "no stable
// identity available" and refuse to persist -- a hash of nothing would collide
// across unrelated records.
[[nodiscard]] constexpr EntityId MakeEntityId(std::string_view acPluginName, std::uint32_t aBaseId) noexcept
{
    if (acPluginName.empty())
        return 0;

    // The BaseId is always folded in as four fixed-width bytes, so the encoding
    // is already unambiguous and the separator is not what prevents collisions.
    // It is kept only to mark the field boundary should a third component ever
    // be appended, where a variable-width field would make it necessary.
    std::uint64_t hash = detail::Fnv1a(acPluginName);
    hash = detail::Fnv1a(std::string_view("|"), hash);
    hash = detail::Fnv1a(aBaseId, hash);

    // 0 is reserved for "no identity", so fold it away in the astronomically
    // unlikely case the hash lands there.
    return hash == 0 ? 1 : hash;
}
} // namespace Halcyon
