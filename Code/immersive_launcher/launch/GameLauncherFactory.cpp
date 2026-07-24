// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include "IGameLauncher.h"
#include "InProcessLauncher.h"
#include "ExternalProcessLauncher.h"

#include <Windows.h>
#include <cstring>

#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <spdlog/spdlog.h>

namespace launcher::launch
{
namespace
{
// wine_get_version só existe no ntdll do Wine; é a checagem canônica que o próprio
// projeto Wine documenta para software que precisa se adaptar.
bool IsRunningUnderWine() noexcept
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return false;

    return GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

const char* GetWineVersion() noexcept
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return nullptr;

    using TWineGetVersion = const char*(__cdecl*)();
    const auto pWineGetVersion = reinterpret_cast<TWineGetVersion>(GetProcAddress(ntdll, "wine_get_version"));

    return pWineGetVersion ? pWineGetVersion() : nullptr;
}
} // namespace

const char* ToString(Strategy aStrategy) noexcept
{
    switch (aStrategy)
    {
    case Strategy::kInProcess: return "in-process (manual PE mapping)";
    case Strategy::kExternalProcess: return "external process (CreateProcess + injection)";
    }

    return "unknown";
}

Strategy SelectDefaultStrategy() noexcept
{
    if (IsRunningUnderWine())
    {
        const char* pVersion = GetWineVersion();
        spdlog::info("[launch] Wine detected (version {}) - manual PE mapping is not usable here", pVersion ? pVersion : "unknown");
        spdlog::info("[launch] reason: unwind tables of a self-mapped image are invisible to RtlVirtualUnwind2");
        return Strategy::kExternalProcess;
    }

    return Strategy::kInProcess;
}

Strategy ParseStrategyOverride(int aArgc, char** apArgv, bool& aOverridden) noexcept
{
    aOverridden = false;

    for (int i = 1; i < aArgc; ++i)
    {
        if (!apArgv[i])
            continue;

        constexpr const char* kFlag = "--launch-mode=";
        const size_t flagLen = std::strlen(kFlag);

        if (std::strncmp(apArgv[i], kFlag, flagLen) != 0)
            continue;

        const char* pValue = apArgv[i] + flagLen;

        if (std::strcmp(pValue, "inprocess") == 0)
        {
            aOverridden = true;
            return Strategy::kInProcess;
        }

        if (std::strcmp(pValue, "external") == 0)
        {
            aOverridden = true;
            return Strategy::kExternalProcess;
        }

        spdlog::warn("[launch] unknown --launch-mode value '{}' (expected 'inprocess' or 'external'); ignoring", pValue);
    }

    return SelectDefaultStrategy();
}

TiltedPhoques::UniquePtr<IGameLauncher> CreateGameLauncher(Strategy aStrategy)
{
    // O UniquePtr do TiltedPhoques carrega um deleter próprio parametrizado pelo
    // tipo, então não há conversão implícita de derivada para base como no
    // std::unique_ptr; CastUnique é o utilitário do projeto para isso (mesmo
    // padrão das factories de mensagem).
    if (aStrategy == Strategy::kExternalProcess)
        return TiltedPhoques::CastUnique<IGameLauncher>(TiltedPhoques::MakeUnique<ExternalProcessLauncher>());

    return TiltedPhoques::CastUnique<IGameLauncher>(TiltedPhoques::MakeUnique<InProcessLauncher>());
}

} // namespace launcher::launch
