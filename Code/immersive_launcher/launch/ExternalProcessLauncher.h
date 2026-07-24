// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.
#pragma once

#include "IGameLauncher.h"

#include <Windows.h>

namespace launcher::launch
{
// Cria o SkyrimSE.exe como processo real e suspenso, injeta o client antes do
// entry point e resume. Necessário sob Wine/Proton, onde o mapeamento manual
// deixa as unwind tables invisíveis para o RtlVirtualUnwind2.
//
// Validado sob GE-Proton11-1: o jogo sobrevive à 0x406D1388 e segue normalmente.
// Ver Code/linux_probe/README.md.
class ExternalProcessLauncher final : public IGameLauncher
{
public:
    ~ExternalProcessLauncher() override;

    bool Prepare(const LaunchRequest& acRequest) override;
    bool Run() override;

    uint32_t GetExitCode() const override { return m_exitCode; }
    Strategy GetStrategy() const override { return Strategy::kExternalProcess; }

private:
    // Escreve o caminho da DLL no processo alvo e chama LoadLibraryW lá.
    bool InjectClient(const std::filesystem::path& acPayloadPath);

    void Cleanup();

    HANDLE m_process = nullptr;
    HANDLE m_mainThread = nullptr;
    HANDLE m_initDoneEvent = nullptr;
    uint32_t m_exitCode = 0;
};
} // namespace launcher::launch
