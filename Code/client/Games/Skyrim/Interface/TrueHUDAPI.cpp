#include <TiltedOnlinePCH.h>

#include <Games/Skyrim/Interface/TrueHUDAPI.h>

namespace TRUEHUD_API
{
IVTrueHUD1* RequestTrueHUDInterface() noexcept
{
    // Resolved once: a missing TrueHUD stays missing for the session, and a
    // present one hands out the same singleton every time.
    static IVTrueHUD1* s_pInterface = []() -> IVTrueHUD1*
    {
        // GetModuleHandleW, never LoadLibrary: if the user does not have
        // TrueHUD, there is nothing to load and nothing to fail.
        const HMODULE pTrueHud = GetModuleHandleW(L"TrueHUD.dll");
        if (!pTrueHud)
        {
            spdlog::info("[truehud] not installed - remote player health bars are disabled");
            return nullptr;
        }

        using TRequestPluginAPI = void* (*)(InterfaceVersion aInterfaceVersion);
        const auto pRequest = reinterpret_cast<TRequestPluginAPI>(GetProcAddress(pTrueHud, "RequestPluginAPI"));
        if (!pRequest)
        {
            spdlog::warn("[truehud] TrueHUD.dll is loaded but does not export RequestPluginAPI");
            return nullptr;
        }

        // V1 is requested on purpose: the interface is versioned by extension,
        // so V1 keeps working against newer TrueHUD builds, and everything used
        // here lives in V1. Asking for a newer version would fail on an older
        // install for no benefit.
        auto* pInterface = static_cast<IVTrueHUD1*>(pRequest(InterfaceVersion::V1));
        if (!pInterface)
        {
            spdlog::warn("[truehud] RequestPluginAPI returned no interface");
            return nullptr;
        }

        spdlog::info("[truehud] interface acquired - remote player health bars are available");
        return pInterface;
    }();

    return s_pInterface;
}

bool HasTargetControl() noexcept
{
    // Requested once. TrueHUD hands the target slot to a single owner, so a
    // refusal is final for the session and must be respected: another mod is
    // driving the target and fighting over it would break both.
    static const bool s_hasControl = []()
    {
        auto* pTrueHud = RequestTrueHUDInterface();
        if (!pTrueHud)
            return false;

        const APIResult cResult = pTrueHud->RequestTargetControl(kSkyrimTogetherPluginHandle);
        if (cResult == APIResult::OK || cResult == APIResult::AlreadyGiven)
        {
            spdlog::info("[truehud] target control granted - remote player health bars will be shown");
            return true;
        }

        spdlog::warn("[truehud] target control unavailable (result {}) - another mod owns it, no health bars", static_cast<uint32_t>(cResult));
        return false;
    }();

    return s_hasControl;
}
} // namespace TRUEHUD_API
