#include "Instrumentation.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#if defined(_WIN32)
#include <TiltedReverse/Code/reverse/include/Debug.hpp>
#endif

namespace launcher::instrumentation
{
namespace
{
constexpr size_t kLogFileSizeCap = 1048576 * 5; // 5 MiB, igual ao servidor
constexpr size_t kLogFileCount = 3;
constexpr char kLogFileName[] = "logs/SkyrimTogether.log";
constexpr char kLogLevelEnv[] = "TE_LOG_LEVEL";

// Nível de log derivado das flags. --debug vence --verbose.
spdlog::level::level_enum ResolveLevel(const Options& aOptions)
{
    if (aOptions.debug)
        return spdlog::level::trace;
    if (aOptions.verbose)
        return spdlog::level::debug;

    // Sem flags: respeita TE_LOG_LEVEL se definido, senão "info".
    // std::getenv é o caminho portável (Windows + Linux); silenciamos o aviso
    // do MSVC sobre "unsafe" localmente, sem afetar outros compiladores.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    if (const char* env = std::getenv(kLogLevelEnv))
    {
        const auto lvl = spdlog::level::from_str(env);
        // from_str devolve "off" para strings desconhecidas; nesse caso caímos no padrão.
        if (lvl != spdlog::level::off || std::strcmp(env, "off") == 0)
            return lvl;
    }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    return spdlog::level::info;
}
} // namespace

Options ParseOptions(int aArgc, char** aArgv)
{
    Options options;

    for (int i = 1; i < aArgc; ++i)
    {
        if (std::strcmp(aArgv[i], "--verbose") == 0)
            options.verbose = true;
        else if (std::strcmp(aArgv[i], "--debug") == 0)
            options.debug = true;
        else if (std::strcmp(aArgv[i], "--dump-config") == 0)
            options.dumpConfig = true;
    }

    return options;
}

void SetupLogging(const Options& aOptions)
{
    using namespace spdlog;

    const auto level = ResolveLevel(aOptions);

#if defined(_WIN32)
    // --debug/--verbose querem saída visível; garante um console mesmo em builds
    // release, onde ele normalmente não é criado.
    if (aOptions.debug || aOptions.verbose)
        TiltedPhoques::Debug::CreateConsole();
#endif

    std::error_code ec;
    std::filesystem::create_directory("logs", ec);

    auto consoleSink = std::make_shared<sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<sinks::rotating_file_sink_mt>(kLogFileName, kLogFileSizeCap, kLogFileCount);

    auto logger = std::make_shared<spdlog::logger>("launcher", sinks_init_list{consoleSink, fileSink});
    logger->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [tid %t] %$ %v");
    logger->set_level(level);
    logger->flush_on(level::warn);

    set_default_logger(logger);
    flush_every(std::chrono::seconds(2));

    spdlog::debug("Logging initialized (level={}, verbose={}, debug={})", to_string_view(level), aOptions.verbose, aOptions.debug);
}

void DumpConfig(const Options& aOptions, const TiltedPhoques::String& aExePath, const TiltedPhoques::String& aGamePath, const TiltedPhoques::String& aVersion)
{
#if defined(_WIN32)
    constexpr const char* kPlatform = "windows";
#else
    constexpr const char* kPlatform = "linux";
#endif

    spdlog::info("---- launcher configuration ----");
    spdlog::info("  platform    : {}", kPlatform);
    spdlog::info("  game path   : {}", aGamePath.empty() ? "<unresolved>" : aGamePath.c_str());
    spdlog::info("  exe path    : {}", aExePath.empty() ? "<unresolved>" : aExePath.c_str());
    spdlog::info("  exe version : {}", aVersion.empty() ? "<unresolved>" : aVersion.c_str());
    spdlog::info("  verbose     : {}", aOptions.verbose);
    spdlog::info("  debug       : {}", aOptions.debug);
    spdlog::info("  dump-config : {}", aOptions.dumpConfig);
    spdlog::info("--------------------------------");
}
} // namespace launcher::instrumentation
