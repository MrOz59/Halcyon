#include "DediRunner.h"

#include <console/CommandSettingsProvider.h>

#include <chrono>
#include <base/threading/ThreadUtils.h>

namespace
{
constexpr char kSettingsFileName[] = "STServer.ini";

DediRunner* s_pRunner{nullptr};
} // namespace

// imports
GS_IMPORT TiltedPhoques::UniquePtr<IGameServerInstance> CreateGameServer(Console::ConsoleRegistry& conReg, const std::function<void()>& aCallback);

// needs to be global
Console::Setting bConsole{"bConsole", "Enable the console", true};

DediRunner* GetDediRunner() noexcept
{
    return s_pRunner;
}

DediRunner::DediRunner(int argc, char** argv)
    : m_console(KCompilerStopThisBullshit)
{
    s_pRunner = this;

    uv_loop_init(&m_loop);

    m_pServerInstance = std::move(CreateGameServer(m_console, [this, argc, argv]() { LoadSettings(argc, argv); }));

    // it is here for now..
    m_pServerInstance->Initialize();
    SaveSettingsToIni(m_console, m_SettingsPath);
}

DediRunner::~DediRunner()
{
    GetTerminalConsole().Shutdown();

    if (m_useIni)
        SaveSettingsToIni(m_console, m_SettingsPath);

    uv_loop_close(&m_loop);
    s_pRunner = nullptr;
}

void DediRunner::LoadSettings(int argc, char** argv)
{
    // If we have line args load, don't use the ini
    if (argc > 1)
    {
        LoadSettingsFromCommand(m_console, argc, argv);
    }
    else
    {
        m_useIni = true;
        m_SettingsPath = fs::current_path() / kConfigPathName / kSettingsFileName;
        if (!exists(m_SettingsPath))
        {
            // There is a bug in here waiting to be found. Since settings are
            // registered later, some server settings may be missing.
            create_directory(fs::current_path() / kConfigPathName);
            SaveSettingsToIni(m_console, m_SettingsPath);
            return;
        }

        LoadSettingsFromIni(m_console, m_SettingsPath);
    }
}

void DediRunner::RunGSThread()
{
    while (m_pServerInstance->IsListening())
    {
        m_pServerInstance->Update();

        if (bConsole)
        {
            uv_run(&m_loop, UV_RUN_NOWAIT);
            m_console.Update();
        }
    }
}

void DediRunner::StartTerminalIO()
{
    auto& terminal = GetTerminalConsole();

    const bool interactive = terminal.Initialize(
        m_loop,
        [this](const std::string& acCommand)
        {
            HandleConsole(acCommand);
        },
        [this]()
        {
            RequestKill();
        });

    if (auto logger = spdlog::get(KCompilerStopThisBullshit))
    {
        logger->info("Halcyon server started. Type /help for a list of commands.");

        if (!interactive)
            logger->info("Interactive terminal features are unavailable; using plain console mode.");
    }
}

void DediRunner::RequestKill()
{
    m_pServerInstance->Shutdown();

#if defined(_WIN32)
    // Work around the Control Handler exception (Control-C) being set while
    // running under a debugger.
    if (IsDebuggerPresent())
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(300ms);
    }
#endif
}

void DediRunner::HandleConsole(const TiltedPhoques::String& acCommand)
{
    if (acCommand.empty())
        return;

    using exr = Console::ConsoleRegistry::ExecutionResult;

    const exr result = m_console.TryExecuteCommand(acCommand);

    if (result == exr::kDirty && m_useIni)
        SaveSettingsToIni(m_console, m_SettingsPath);
}
