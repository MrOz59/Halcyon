
#include <TiltedReverse/Code/reverse/include/Debug.hpp>
#include "TargetConfig.h"
#include "launcher.h"

#include "loader/ExeLoader.h"
#include "loader/PathRerouting.h"

#include "launch/IGameLauncher.h"

#include "Utils/Error.h"
#include "Utils/FileVersion.inl"

#include "oobe/PathSelection.h"
#include "oobe/PathArgument.h"
#include "oobe/SupportChecks.h"
#include "steam/SteamLoader.h"

#include "base/dialogues/win/TaskDialog.h"
#include "utils/Registry.h"
#include "Instrumentation.h"

#include <spdlog/spdlog.h>

#include <BranchInfo.h>


// Defined in EarlyLoad.dll
bool __declspec(dllimport) EarlyInstallSucceeded();

HICON g_SharedWindowIcon = nullptr;

namespace launcher
{
static LaunchContext* g_context = nullptr;

LaunchContext* GetLaunchContext()
{
#if 0
    if (!g_context)
        __debugbreak();
#endif
    return g_context;
}

bool LaunchContext::GetLoaded()
{
    return isLoaded;
}

// Everything is nothing, life is worth living, just look to the stars
#define DIE_NOW(err)  \
    {                 \
        Die(err);     \
        return false; \
    }

void SetMaxstdio()
{
    const auto handle = GetModuleHandleW(L"API-MS-WIN-CRT-STDIO-L1-1-0.DLL");
    if (!handle)
        return;

    const auto setmaxstdioFunc = reinterpret_cast<decltype(&_setmaxstdio)>(GetProcAddress(handle, "_setmaxstdio"));

    if (!setmaxstdioFunc)
        return;

    setmaxstdioFunc(8192);
}

int StartUp(int argc, char** argv)
{
    bool askSelect = (GetAsyncKeyState(VK_SPACE) & 0x8000);
    if (!HandleArguments(argc, argv, askSelect))
        return -1;

    // TODO(Force): Make some InitSharedResources func.
    g_SharedWindowIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(102));

#if (!IS_MASTER)
    TiltedPhoques::Debug::CreateConsole();
#endif

    SetMaxstdio();

    if (!EarlyInstallSucceeded())
        DIE_NOW(L"Early load install failed. Tell Force about this.");

    auto LC = std::make_unique<LaunchContext>();
    g_context = LC.get();

    {
        const wchar_t* ec = nullptr;
        const auto status = oobe::ReportModCompatabilityStatus();
        switch (status)
        {
        case oobe::CompatabilityStatus::kDX11Unsupported: ec = L"Device does not support DirectX 11"; break;
        case oobe::CompatabilityStatus::kOldOS: ec = L"Operating system unsupported. Please upgrade to Windows 8.1 or greater"; break;
        }

        if (ec)
            DIE_NOW(ec);
    }

    if (!oobe::SelectInstall(askSelect))
        DIE_NOW(L"Failed to select game install.");

    spdlog::info("Game install selected: {}", LC->gamePath.string());

    // Bind path environment.
    loader::InstallPathRouting(LC->gamePath);
    steam::Load(LC->gamePath);

    LC->Version = QueryFileVersion(LC->exePath.c_str());
    if (LC->Version.empty())
        DIE_NOW(L"Failed to query game version");

    // Fase 1 (instrumentação): com os caminhos e a versão já resolvidos, imprime
    // a configuração. Com --dump-config, encerra aqui sem iniciar o jogo.
    {
        const auto diagOptions = instrumentation::ParseOptions(argc, argv);
        const auto toStr = [](const std::filesystem::path& p) { return TiltedPhoques::String(p.string()); };
        instrumentation::DumpConfig(diagOptions, toStr(LC->exePath), toStr(LC->gamePath), LC->Version);
        if (diagOptions.dumpConfig)
        {
            spdlog::info("--dump-config specified; exiting before game start.");
            return 0;
        }
    }

    // Sob Wine/Proton o mapeamento manual de PE não é utilizável (as unwind tables
    // do módulo auto-mapeado ficam invisíveis para o RtlVirtualUnwind2), então a
    // estratégia padrão muda para processo externo. No Windows nada muda.
    bool strategyOverridden = false;
    const auto strategy = launch::ParseStrategyOverride(argc, argv, strategyOverridden);

    spdlog::info("[launch] strategy: {}{}", launch::ToString(strategy), strategyOverridden ? " (forced via --launch-mode)" : "");

    auto gameLauncher = launch::CreateGameLauncher(strategy);

    // No modo in-process o launcher passa a se comportar como o jogo perante o
    // GetModuleFileName*(nullptr); no modo externo o jogo é um processo separado e
    // essa fachada não se aplica.
    if (strategy == launch::Strategy::kInProcess)
        LC->SetLoaded();

    const launch::LaunchRequest request{LC->exePath, LC->gamePath, LC->Version};

    if (!gameLauncher->Prepare(request))
        return 3;

    spdlog::info("Program prepared, entering game.");
    spdlog::default_logger()->flush();

    // Só retorna quando o jogo termina.
    if (!gameLauncher->Run())
        return 4;

    spdlog::info("[boot] game exited (code {})", gameLauncher->GetExitCode());
    spdlog::default_logger()->flush();
    return 0;
}

bool HandleArguments(int aArgc, char** aArgv, bool& aAskSelect)
{
    for (int i = 1; i < aArgc; i++)
    {
        if (std::strcmp(aArgv[i], "-r") == 0)
            aAskSelect = true;
        else if (std::strcmp(aArgv[i], "--exePath") == 0)
        {
            if (i + 1 >= aArgc)
            {
                SetLastError(ERROR_BAD_PATHNAME);
                Die(L"No exe path specified", true);
                return false;
            }

            if (!oobe::PathArgument(aArgv[i + 1]))
            {
                SetLastError(ERROR_BAD_ARGUMENTS);
                Die(L"Failed to parse path argument", true);
                return false;
            }
        }
    }

    return true;
}
} // namespace launcher

// CreateProcess in suspended mode.
// Inject usvfs_64.dll -> invoke InitHooks
// (https://github.com/ModOrganizer2/usvfs/blob/f8051c179dee114b7e06c5dab2482977c285d611/src/usvfs_dll/usvfs.cpp#L352)
// Resume proc

// InjectDLLRemoteThread ->SkipInit
