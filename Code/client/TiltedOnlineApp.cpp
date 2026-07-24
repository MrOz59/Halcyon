#include <TiltedOnlinePCH.h>

#include <TiltedOnlineApp.h>

#include <DInputHook.hpp>
#include <dinput.h>
#include <WindowsHook.hpp>

#include <World.h>

#include <cstdio>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Systems/RenderSystemD3D11.h>

#include <Services/OverlayService.h>
#include <Services/ImguiService.h>
#include <Services/DiscordService.h>

#include <ScriptExtender.h>
#include <NvidiaUtil.h>

using TiltedPhoques::Debug;

TiltedOnlineApp::TiltedOnlineApp()
{
    // Set console code page to UTF-8 so console known how to interpret string data
    SetConsoleOutputCP(CP_UTF8);

    auto logPath = TiltedPhoques::GetPath() / "logs";

    std::error_code ec;
    create_directory(logPath, ec);

    auto rotatingLogger = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPath / "tp_client.log", 1048576 * 5, 3);
    // rotatingLogger->set_level(spdlog::level::debug);
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("", spdlog::sinks_init_list{console, rotatingLogger});
    logger->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [tid %t] %$ %v");
    spdlog::flush_every(std::chrono::seconds(1));
    set_default_logger(logger);
}

TiltedOnlineApp::~TiltedOnlineApp() = default;

void* TiltedOnlineApp::GetMainAddress() const
{
    POINTER_SKYRIMSE(void, winMain, 36544);

    return winMain.GetPtr();
}

// Diagnóstico do port Linux: log síncrono (WriteFile + FlushFileBuffers) que
// sobrevive a um crash imediato do processo, ao contrário do spdlog em buffer.
// Grava ao lado da DLL do client. Remover quando o crash 0x80000003 na init do
// jogo estiver resolvido.
static void DiagStep(const char* apStep)
{
    static HANDLE s_hFile = INVALID_HANDLE_VALUE;
    if (s_hFile == INVALID_HANDLE_VALUE)
    {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(GetModuleHandleW(L"STClientPayload.dll"), modulePath, MAX_PATH);
        std::filesystem::path p = modulePath[0] ? std::filesystem::path(modulePath).parent_path() : std::filesystem::current_path();
        const auto logPath = (p / "st_beginmain_diag.log").wstring();
        s_hFile = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (s_hFile == INVALID_HANDLE_VALUE)
        return;

    char line[256];
    const int n = _snprintf_s(line, _TRUNCATE, "[beginmain] %s\r\n", apStep);
    DWORD written = 0;
    WriteFile(s_hFile, line, static_cast<DWORD>(n), &written, nullptr);
    FlushFileBuffers(s_hFile);
}

bool TiltedOnlineApp::BeginMain()
{
    DiagStep("enter");

    World::Create();
    DiagStep("World::Create done");

    World::Get().ctx().at<DiscordService>().Init();
    DiagStep("DiscordService::Init done");

    World::Get().ctx().emplace<RenderSystemD3D11>(World::Get().ctx().at<OverlayService>(), World::Get().ctx().at<ImguiService>());
    DiagStep("RenderSystemD3D11 done");

    LoadScriptExender();
    DiagStep("LoadScriptExender done");

    // TODO: Figure out a way to un-blacklist NvCamera64.dll (see DllBlocklist.cpp). Then this hack can be removed
    if (IsNvidiaOverlayLoaded())
        ApplyNvidiaFix();

    DiagStep("BeginMain complete");
    return true;
}

bool TiltedOnlineApp::EndMain()
{
    UninstallHooks();
    if (m_pDevice)
        m_pDevice->Release();

    return true;
}

void TiltedOnlineApp::Update()
{
    // Reverting a change that used to be here to disable bUseFaceGenPreprocessedHeads==true (which is 
    // the default) handling. Extensive testing over months by multiple parties showed that enabling 
    // the flag introduces no issues WITH PROPERLY GENERATED CHARACTERS (in-game character generation 
    // or showracemenu). The shortcut of  "coc riverwood" from the main menu skips proper character generation.
    // 
    // Plus, having it on  has some benefits like helping with neck seams. Comment to avoid revisiting.
    // 
    // There are still some issues to track down, like hair color and maybe face tint not syncing correctly,
    // but they are unrelated and unchanged by this flag.
    // 
 
    // Make sure the window stays active
    POINTER_SKYRIMSE(uint32_t, bAlwaysActive, 380768);

    *bAlwaysActive = 1;

    World::Get().Update();
}

bool TiltedOnlineApp::Attach()
{
    TiltedPhoques::Debug::OnAttach();

    // TiltedPhoques::Nop(0x1405D3FA1, 6);
    return true;
}

bool TiltedOnlineApp::Detach()
{
    TiltedPhoques::Debug::OnDetach();
    return true;
}

void TiltedOnlineApp::InstallHooks2()
{
    TiltedPhoques::Initializer::RunAll();

    TiltedPhoques::DInputHook::Install();
    TiltedPhoques::DInputHook::Get().SetToggleKeys({DIK_F2, DIK_RCONTROL});
}

void TiltedOnlineApp::UninstallHooks()
{
}

void TiltedOnlineApp::ApplyNvidiaFix() noexcept
{
    auto d3dFeatureLevelOut = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = CreateEarlyDxDevice(&m_pDevice, &d3dFeatureLevelOut);
    if (FAILED(hr))
        spdlog::error("D3D11CreateDevice failed. Detected an NVIDIA GPU, error code={0:x}", hr);

    if (d3dFeatureLevelOut < D3D_FEATURE_LEVEL_11_0)
        spdlog::warn("Unexpected D3D11 feature level detected (< 11.0), may cause issues");
}
