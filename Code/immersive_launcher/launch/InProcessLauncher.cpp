// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include "InProcessLauncher.h"
#include "TargetConfig.h"
#include "Utils/Error.h"

#include <Windows.h>

#include <TiltedCore/Filesystem.hpp>
#include <spdlog/spdlog.h>

// Definidos no client (skyrimtogetherclient).
extern void InstallStartHook();
extern void RunTiltedInit(const std::filesystem::path& acGamePath, const TiltedPhoques::String& aExeVersion);

namespace launcher::launch
{
namespace
{
// Diagnóstico: sob Wine o salto para o entry point auto-mapeado pode disparar uma
// exceção que mata o processo sem rastro. Logamos código e endereço antes de
// deixá-la seguir. Função separada e sem objetos com destrutor, como exige a
// mistura C++/SEH.
int FilterGameException(unsigned long aCode, void* apInfo)
{
    auto* pInfo = static_cast<EXCEPTION_POINTERS*>(apInfo);
    void* faultAddr = pInfo && pInfo->ExceptionRecord ? pInfo->ExceptionRecord->ExceptionAddress : nullptr;
    spdlog::critical("[boot] SEH exception in gameMain: code=0x{:08x} at address=0x{:x}", aCode, reinterpret_cast<uintptr_t>(faultAddr));

    if (aCode == EXCEPTION_ACCESS_VIOLATION && pInfo && pInfo->ExceptionRecord && pInfo->ExceptionRecord->NumberParameters >= 2)
    {
        const auto op = pInfo->ExceptionRecord->ExceptionInformation[0];
        const auto addr = pInfo->ExceptionRecord->ExceptionInformation[1];
        spdlog::critical("[boot]   access violation {} address 0x{:x}", op == 0 ? "reading" : (op == 1 ? "writing" : "executing"), addr);
    }

    spdlog::default_logger()->flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

void RunGameMainGuarded(ExeLoader::TEntryPoint aGameMain)
{
    __try
    {
        aGameMain();
    }
    __except (FilterGameException(GetExceptionCode(), GetExceptionInformation()))
    {
    }
}
} // namespace

bool InProcessLauncher::Prepare(const LaunchRequest& acRequest)
{
    auto content = TiltedPhoques::LoadFile(acRequest.exePath);
    if (content.empty())
    {
        Die(L"Failed to mount game executable");
        return false;
    }

    ExeLoader loader(CurrentTarget.exeLoadSz);
    if (!loader.Load(reinterpret_cast<uint8_t*>(content.data())))
    {
        Die(L"Fatal error while mapping executable");
        return false;
    }

    m_gameMain = loader.GetEntryPoint();
    return m_gameMain != nullptr;
}

bool InProcessLauncher::Run()
{
    if (!m_gameMain)
        return false;

    // No modo in-process o client entra pelo hook de IAT em GetStartupInfoW, que o
    // CRT do jogo chama durante o startup (ver stubs/CrtStartupHooks.cpp).
    spdlog::info("[boot] calling InstallStartHook()");
    spdlog::default_logger()->flush();
    InstallStartHook();

    spdlog::info("[boot] RunTiltedInit() returned; jumping into game entry point (gameMain)");
    spdlog::default_logger()->flush();

    RunGameMainGuarded(m_gameMain);

    spdlog::info("[boot] gameMain() returned (game exited)");
    spdlog::default_logger()->flush();
    return true;
}

} // namespace launcher::launch
