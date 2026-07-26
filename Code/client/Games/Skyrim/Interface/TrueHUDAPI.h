#pragma once

// Adapted declaration of TrueHUD's public plugin API (TRUEHUD_API::IVTrueHUD1).
//
// TrueHUD is an OPTIONAL dependency. There is no import library and no build
// dependency: the interface is fetched at runtime from an already loaded
// TrueHUD.dll and every call site is a no-op when the mod is absent. That is
// also how TrueHUD's own helper behaves - it returns nullptr when the DLL is
// not there - so "no TrueHUD" simply means "no health bars", never a failure.
//
// The upstream header (ersh1/TrueHUD, src/TrueHUDAPI.h, which explicitly allows
// copying into other projects) is written against CommonLibSSE, which this
// project does not use. Only the declarations needed here are reproduced, with
// this project's own types:
//
//   RE::ActorHandle   ->  BSPointerHandle<TESObjectREFR>
//   SKSE::PluginHandle->  uint32_t
//
// Both are a single uint32_t and are passed in a register, so the ABI matches.
//
// CRITICAL: this is a vtable-compatible declaration. The order and the
// signatures below are copied verbatim from the upstream header and MUST NOT be
// reordered or altered - AddActorInfoBar is slot 6 and RemoveActorInfoBar is
// slot 7. Methods after those are declared only so the indices stay correct;
// the ones taking types this project lacks are left as opaque slots because
// they are never called. Adding or removing anything shifts the vtable and
// would call the wrong function.

#include <cstdint>

struct TESObjectREFR;

namespace TRUEHUD_API
{
enum class InterfaceVersion : uint8_t
{
    V1,
    V2,
    V3,
    V4
};

enum class APIResult : uint8_t
{
    OK,
    NotOwner,
    MustKeep,
    AlreadyGiven,
    AlreadyTaken,
    WidgetFailedToLoad,
    BadThread,
};

enum class WidgetRemovalMode : uint8_t
{
    Immediate,
    Normal,
    Delayed
};

using PluginHandle = uint32_t;
using ActorHandle = BSPointerHandle<TESObjectREFR>;

class IVTrueHUD1
{
public:
    // 1
    [[nodiscard]] virtual unsigned long GetTrueHUDThreadId() const noexcept = 0;
    // 2
    [[nodiscard]] virtual APIResult RequestTargetControl(PluginHandle aMyPluginHandle) noexcept = 0;
    // 3
    [[nodiscard]] virtual APIResult RequestSpecialResourceBarsControl(PluginHandle aMyPluginHandle) noexcept = 0;
    // 4
    virtual APIResult SetTarget(PluginHandle aMyPluginHandle, ActorHandle aActorHandle) noexcept = 0;
    // 5
    virtual APIResult SetSoftTarget(PluginHandle aMyPluginHandle, ActorHandle aActorHandle) noexcept = 0;

    // 6 - shows the name + health bar above an actor, as done for any NPC.
    virtual void AddActorInfoBar(ActorHandle aActorHandle) noexcept = 0;
    // 7
    virtual void RemoveActorInfoBar(ActorHandle aActorHandle, WidgetRemovalMode aRemovalMode) noexcept = 0;

    // 8
    virtual void AddBoss(ActorHandle aActorHandle) noexcept = 0;
    // 9
    virtual void RemoveBoss(ActorHandle aActorHandle, WidgetRemovalMode aRemovalMode) noexcept = 0;
    // 10 - takes RE::ActorValue (an enum, 32 bit) upstream.
    virtual void FlashActorValue(ActorHandle aActorHandle, uint32_t aActorValue, bool aLong) noexcept = 0;

    // 11 onwards are never called from here. Upstream some of them take
    // std::function and std::shared_ptr parameters whose types are not
    // reproduced; the slots exist purely to preserve the vtable layout.
    virtual APIResult Slot11_FlashActorSpecialBar(PluginHandle, ActorHandle, bool) noexcept = 0;
    virtual APIResult Slot12_RegisterSpecialResourceFunctions(PluginHandle, void*, void*, bool, bool) noexcept = 0;
    virtual void Slot13_LoadCustomWidgets(PluginHandle, const void*, void*) noexcept = 0;
    virtual void Slot14_RegisterNewWidgetType(PluginHandle, uint32_t) noexcept = 0;
    virtual void Slot15_AddWidget(PluginHandle, uint32_t, uint32_t, const void*, void*) noexcept = 0;
    virtual void Slot16_RemoveWidget(PluginHandle, uint32_t, uint32_t, WidgetRemovalMode) noexcept = 0;
    virtual PluginHandle Slot17_GetTargetControlOwner() const noexcept = 0;
    virtual PluginHandle Slot18_GetPlayerWidgetBarColorsControlOwner() const noexcept = 0;
    virtual PluginHandle Slot19_GetSpecialResourceBarControlOwner() const noexcept = 0;
    virtual APIResult Slot20_ReleaseTargetControl(PluginHandle) noexcept = 0;
    virtual APIResult Slot21_ReleaseSpecialResourceBarControl(PluginHandle) noexcept = 0;
};

/**
 * @brief Fetch TrueHUD's API, or nullptr when the mod is not installed.
 *
 * Mirrors the upstream helper: resolve the export from the loaded DLL and call
 * it. Never loads the library itself, so a missing TrueHUD costs nothing.
 */
[[nodiscard]] IVTrueHUD1* RequestTrueHUDInterface() noexcept;
} // namespace TRUEHUD_API
