#pragma once

// spdlog
#include <spdlog/spdlog.h>

#include <thread>

#include <BuildInfo.h>
#include <uv.h>
#include <console/ConsoleRegistry.h>
#include <console/IniSettingsProvider.h>
#include <common/GameServerInstance.h>

#include "TerminalConsole.h"

#ifdef _WIN32
#define GS_IMPORT extern __declspec(dllimport)
#else
#define GS_IMPORT extern __attribute__((visibility("default")))
#endif

namespace fs = std::filesystem;

static constexpr char kConfigPathName[] = "config";
static constexpr char KCompilerStopThisBullshit[] = "ConOut";
static constexpr char kBuildTag[]{BUILD_BRANCH "@" BUILD_COMMIT};

// Frontend class for the dedicated terminal based server
struct DediRunner
{
    DediRunner(int argc, char** argv);
    ~DediRunner();

    void RunGSThread();
    void StartTerminalIO();
    void RequestKill();
    void HandleConsole(const TiltedPhoques::String& acCommand);

private:
    void LoadSettings(int argc, char** argv);

private:
    // Order here matters for constructor calling order.
    uv_loop_t m_loop;
    fs::path m_SettingsPath;
    bool m_useIni{false};
    Console::ConsoleRegistry m_console;
    TiltedPhoques::UniquePtr<IGameServerInstance> m_pServerInstance;
};

DediRunner* GetDediRunner() noexcept;
