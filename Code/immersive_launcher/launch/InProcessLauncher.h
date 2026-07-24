// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.
#pragma once

#include "IGameLauncher.h"
#include "loader/ExeLoader.h"

namespace launcher::launch
{
// Caminho histórico: mapeia o PE à mão no processo do launcher e salta para o
// entry point. Preservado sem mudança de comportamento — é o padrão no Windows.
class InProcessLauncher final : public IGameLauncher
{
public:
    bool Prepare(const LaunchRequest& acRequest) override;
    bool Run() override;

    uint32_t GetExitCode() const override { return m_exitCode; }
    Strategy GetStrategy() const override { return Strategy::kInProcess; }

private:
    ExeLoader::TEntryPoint m_gameMain = nullptr;
    uint32_t m_exitCode = 0;
};
} // namespace launcher::launch
