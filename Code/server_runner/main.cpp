#include <TiltedCore/Filesystem.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <spdlog/sinks/rotating_file_sink.h>

#include <Setting.h>
#include <base/simpleini/SimpleIni.h>
#include <base/threading/ThreadUtils.h>

#include "DediRunner.h"
#include "TerminalConsole.h"

#ifdef _WIN32
#include <base/dialogues/win/TaskDialog.h>
#pragma comment(lib, "Comctl32.lib")
#elif defined(__linux__)
#include <signal.h>
#endif

namespace
{
constexpr char kLogFileName[] = "STServerOut.log";
// Its fine for us if several potential server instances read this, since its a tilted platform thing
// and therefore not considered game specific.
constexpr char kEULAName[] = "EULA.txt";
constexpr char kEULAText[] = ";Please indicate your agreement to the Tilted platform service agreement\n"
                             ";by setting bConfirmEULA to true\n"
                             "[EULA]\n";
constexpr char kEULATextFalse[] = "bConfirmEULA=false";
constexpr char kEULATextTrue[] = "bConfirmEULA=true";

namespace fs = std::filesystem;

Console::StringSetting sLogLevel{"sLogLevel", "Log level to print", "info"};
using namespace std::chrono_literals;
} // namespace

extern Console::Setting<bool> bConsole;

GS_IMPORT void SetDefaultLogger(std::shared_ptr<spdlog::logger> aLogger);
GS_IMPORT void RegisterLogger(std::shared_ptr<spdlog::logger> aLogger);

struct LogInstance
{
    static constexpr size_t kLogFileSizeCap = 1048576 * 5;

    LogInstance()
    {
        using namespace spdlog;

        std::error_code ec;
        fs::create_directory("logs", ec);

        const auto terminalSink = MakeTerminalConsoleSink();

        auto consoleOut = std::make_shared<logger>(KCompilerStopThisBullshit, terminalSink);
        consoleOut->set_pattern("%v");
        spdlog::register_logger(consoleOut);

        // Make the server library aware of the command-output logger.
        RegisterLogger(consoleOut);

        auto fileOut = std::make_shared<sinks::rotating_file_sink_mt>(
            std::string("logs/") + kLogFileName,
            kLogFileSizeCap,
            3);

        auto globalOut = std::make_shared<logger>("", sinks_init_list{terminalSink, fileOut});
        globalOut->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [tid %t]%$ %v");
        globalOut->set_level(level::from_str(sLogLevel.value()));

        spdlog::flush_every(std::chrono::seconds(2));
        spdlog::set_default_logger(globalOut);

        // Also make the server library aware of the default logger.
        SetDefaultLogger(globalOut);
    }

    ~LogInstance() { spdlog::shutdown(); }
};

static bool RegisterQuitHandler()
{
#if defined(_WIN32)
    static auto CtrlHandler = ([](DWORD aType) {
        switch (aType)
        {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT: {
            if (auto* pRunner = GetDediRunner())
            {
                pRunner->RequestKill();
                return TRUE;
            }
            // If the user kills during the constructor, deny the request to
            // avoid destroying partially initialized state.
            return FALSE;
        }
        default:
            return FALSE;
        }
    });

    return SetConsoleCtrlHandler(CtrlHandler, TRUE);

#elif defined(__linux__)
    static auto CtrlHandler = ([](int) {
        if (auto* pRunner = GetDediRunner())
            pRunner->RequestKill();
    });

    signal(SIGINT, CtrlHandler);
    signal(SIGTERM, CtrlHandler);
    return true;
#else
    return true;
#endif
}

#ifdef _WIN32
static bool ShowEULADialog()
{
    Base::TaskDialog dia(
        LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(102)),
        L"Tilted Platform Agreement",
        L"Confirm the Tilted Platform EULA",
        L"TODO: Link to EULA",
        nullptr);

    dia.AppendButton(100, L"Accept EULA");
    dia.AppendButton(101, L"Deny EULA");
    dia.SetDefaultButton(101 /* So they have to think about it */);

    return dia.Show() == 100;
}
#endif

// The EULA can be accepted in three ways:
// - Confirm the dialog on startup (Windows only)
// - Signal agreement using an environment variable
// - Set bConfirmEULA in EULA.txt to true
static bool IsEULAAccepted()
{
    const auto path = fs::current_path() / kConfigPathName / kEULAName;

    bool preAccept = false;
    if (char* pValue = std::getenv("TILTED_ACCEPT_EULA"))
    {
        std::string_view env(pValue);
        preAccept = env == "true" || env == "1" || env == "TRUE";
    }

    auto saveFile = [&]()
    {
#ifdef _WIN32
        if (!preAccept)
            preAccept = ShowEULADialog();
#endif
        fs::create_directory(fs::current_path() / kConfigPathName);

        TiltedPhoques::String eulaText = kEULAText;
        eulaText += preAccept ? kEULATextTrue : kEULATextFalse;

        TiltedPhoques::SaveFile(path, eulaText);
        return preAccept;
    };

    if (!exists(path))
        return saveFile();

    const auto data = TiltedPhoques::LoadFile(path);

    CSimpleIni si;
    if (si.LoadData(data.c_str()) != SI_OK)
        return preAccept;

    if (!si.GetBoolValue("EULA", "bConfirmEULA", false))
    {
#ifdef _WIN32
        preAccept = false;
        return saveFile();
#else
        return false;
#endif
    }

    return true;
}

GS_IMPORT bool CheckBuildTag(const char* apBuildTag);

void ConfigureConsoleMode()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int main(int argc, char** argv)
{
    ConfigureConsoleMode();

    // The binaries are not from the same commit.
    if (!CheckBuildTag(kBuildTag))
        return 1;

    Base::SetCurrentThreadName("ServerRunnerMain");

    LogInstance logger;
    (void)logger;

    // Interactive Ctrl+C is handled by TerminalConsole. The legacy OS signal
    // handler remains disabled because it calls non-signal-safe server code.

    // Keep stack free.
    const auto cpRunner{std::make_unique<DediRunner>(argc, argv)};

    // LogInstance is constructed before the runner reads STServer.ini, so the
    // logger was configured from sLogLevel's compiled-in default. Reapply it
    // now that the file has been loaded, otherwise the configured level is
    // silently ignored.
    if (const auto defaultLogger = spdlog::default_logger())
    {
        const std::string configuredName = sLogLevel.value();
        const auto configured = spdlog::level::from_str(configuredName);

        // from_str yields level::off for anything it does not recognise, so a
        // typo would silence the server entirely. Only accept "off" when that
        // is what was actually written.
        if (configured == spdlog::level::off && configuredName != "off")
        {
            defaultLogger->warn("Unknown sLogLevel '{}', keeping {}", configuredName,
                                spdlog::level::to_string_view(defaultLogger->level()));
        }
        else if (configured != defaultLogger->level())
        {
            defaultLogger->set_level(configured);
            defaultLogger->info("Log level set to {}", configuredName);
        }
    }

    if (bConsole)
        cpRunner->StartTerminalIO();

    cpRunner->RunGSThread();

    return 0;
}
