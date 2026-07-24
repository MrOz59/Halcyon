// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.
//
// Payload injetado no processo do jogo pelo ExternalProcessLauncher.
//
// No modo in-process o launcher e o jogo compartilham o processo, então o client
// era inicializado direto pelo Launcher. No modo externo o jogo é um processo
// separado (necessário sob Wine/Proton, ver Code/linux_probe/README.md) e esta
// DLL é o que leva o client para dentro dele.
//
// A DLL é injetada com a thread principal do jogo ainda SUSPENSA — a mesma janela
// que o SKSE usa — então os hooks são instalados antes de qualquer código do jogo
// rodar.

#include <Windows.h>

#include <filesystem>
#include <string>

#include <TiltedCore/Stl.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "PayloadSupport.h"

// Definidos no client (skyrimtogetherclient), linkado estaticamente aqui.
extern void InstallStartHook();
extern void RunTiltedInit(const std::filesystem::path& acGamePath, const TiltedPhoques::String& aExeVersion);

// O client referencia este ícone (compartilhado com o launcher no modo in-process).
// Aqui não há janela do launcher, então fica nulo e os diálogos usam o padrão.
HICON g_SharedWindowIcon = nullptr;

namespace
{
// Configuração passada pelo launcher via ambiente herdado (ver
// ExternalProcessLauncher.cpp).
constexpr const wchar_t* kEnvGamePath = L"ST_GAME_PATH";
constexpr const wchar_t* kEnvExeVersion = L"ST_EXE_VERSION";

// Evento nomeado usado para avisar o launcher que a init terminou. A DLL não pode
// esperar a init dentro do DllMain (deadlock no loader lock), então quem espera é
// o launcher, antes de resumir a thread principal do jogo.
constexpr const wchar_t* kInitDoneEventName = L"Local\\SkyrimTogether_ClientInitDone";

// Diagnóstico do port Linux: sob Proton o proton run engole PROTON_LOG/WINEDEBUG,
// então instalamos um vectored handler que loga exceções fatais (com módulo e
// offset) direto no log do payload. É a única via confiável para localizar de
// onde vem o crash 0x80000003 (STATUS_BREAKPOINT) observado ~5s após o resume.
// Remover quando o crash estiver resolvido.
PVOID g_pDiagHandler = nullptr;

void DescribeAddress(void* apAddress, char* apBuffer, size_t aBufferSize)
{
    HMODULE hModule = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(apAddress), &hModule) && hModule)
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(hModule, modulePath, MAX_PATH))
        {
            const std::filesystem::path p(modulePath);
            const auto name = p.filename().string();
            const auto offset = reinterpret_cast<uintptr_t>(apAddress) - reinterpret_cast<uintptr_t>(hModule);
            _snprintf_s(apBuffer, aBufferSize, _TRUNCATE, "%s+0x%llx (base 0x%llx)", name.c_str(), static_cast<unsigned long long>(offset), reinterpret_cast<unsigned long long>(hModule));
            return;
        }
    }
    _snprintf_s(apBuffer, aBufferSize, _TRUNCATE, "<no module>");
}

LONG CALLBACK DiagVectoredHandler(EXCEPTION_POINTERS* apInfo)
{
    const auto code = apInfo->ExceptionRecord->ExceptionCode;

    // Só interessam exceções fatais/não-continuáveis. Ignora as informativas que o
    // jogo dispara em volume (nomeação de thread etc.) para não poluir.
    const bool isFatal = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_BREAKPOINT || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW ||
                         code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == 0xC0000409 /* fastfail */;
    if (!isFatal)
        return EXCEPTION_CONTINUE_SEARCH;

    char faultDesc[512]{};
    DescribeAddress(apInfo->ExceptionRecord->ExceptionAddress, faultDesc, sizeof(faultDesc));

    char ripDesc[512]{};
    DescribeAddress(reinterpret_cast<void*>(apInfo->ContextRecord->Rip), ripDesc, sizeof(ripDesc));

    spdlog::critical("[diag] fatal exception code=0x{:08x} tid={}", code, GetCurrentThreadId());
    spdlog::critical("[diag]   fault addr = {}  {}", apInfo->ExceptionRecord->ExceptionAddress, faultDesc);
    spdlog::critical("[diag]   rip        = 0x{:x}  {}", apInfo->ContextRecord->Rip, ripDesc);
    spdlog::critical("[diag]   rsp = 0x{:x}  rbp = 0x{:x}", apInfo->ContextRecord->Rsp, apInfo->ContextRecord->Rbp);
    spdlog::default_logger()->flush();

    return EXCEPTION_CONTINUE_SEARCH;
}

std::wstring ReadEnv(const wchar_t* apName)
{
    std::wstring buffer;
    buffer.resize(32768);

    const DWORD len = GetEnvironmentVariableW(apName, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0 || len > buffer.size())
        return {};

    buffer.resize(len);
    return buffer;
}

void SetupLogging()
{
    try
    {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(GetModuleHandleW(L"STClientPayload.dll"), modulePath, MAX_PATH);

        const auto logPath = std::filesystem::path(modulePath).parent_path() / "st_client_payload.log";

        auto logger = spdlog::basic_logger_mt("payload", logPath.string(), true);
        logger->set_pattern("[%H:%M:%S.%e] [%l] %v");
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
    }
    catch (const std::exception&)
    {
        // Sem log não é motivo para abortar a inicialização do client.
    }
}

// Roda fora do loader lock: DllMain não pode instalar hooks nem carregar módulos
// com segurança, e RunTiltedInit faz as duas coisas.
// Avisa o launcher que a init terminou (com sucesso ou não). O launcher espera
// por isso antes de resumir o jogo; sem o sinal ele segue no timeout.
void SignalInitDone()
{
    if (HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, kInitDoneEventName))
    {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

DWORD WINAPI InitThread(LPVOID)
{
    SetupLogging();

    // Handler de diagnóstico primeiro, para capturar qualquer crash desde a init.
    g_pDiagHandler = AddVectoredExceptionHandler(1, DiagVectoredHandler);

    spdlog::info("[payload] attached to game process (pid={}) diag_handler={}", GetCurrentProcessId(), g_pDiagHandler ? "ok" : "FAILED");

    const auto gamePathStr = ReadEnv(kEnvGamePath);
    const auto exeVersionStr = ReadEnv(kEnvExeVersion);

    if (gamePathStr.empty())
    {
        spdlog::critical("[payload] {} not set - cannot initialize client", "ST_GAME_PATH");
        SignalInitDone();
        return 1;
    }

    const std::filesystem::path gamePath = gamePathStr;
    const TiltedPhoques::String exeVersion(exeVersionStr.begin(), exeVersionStr.end());

    if (!InitializePayloadSupport(gamePath, exeVersion))
    {
        spdlog::critical("[payload] failed to reserve RIP-relative stub memory");
        SignalInitDone();
        return 1;
    }

    spdlog::info("[payload] game path   : {}", gamePath.string());
    spdlog::info("[payload] exe version : {}", exeVersion.c_str());

    // O mapeador in-process deixava o código do jogo gravável. No processo
    // externo, o loader do Windows/Wine aplica RX à seção .text; os helpers
    // legados Put/Nop/SwapCall precisam de escrita temporária para instalar os
    // patches antes de a thread principal do jogo ser retomada.
    if (!EnableGameCodePatching())
    {
        spdlog::critical("[payload] failed to enable game code patching");
        SignalInitDone();
        return 1;
    }
    spdlog::info("[payload] game code patching enabled");

    // Mesma ordem do caminho in-process: o hook de startup primeiro, para que o
    // client seja ativado quando o CRT do jogo chamar GetStartupInfoW.
    spdlog::info("[payload] installing start hook");
    InstallStartHook();

    spdlog::info("[payload] running client init");
    spdlog::default_logger()->flush();
    RunTiltedInit(gamePath, exeVersion);
    spdlog::info("[payload] RunTiltedInit returned");
    spdlog::default_logger()->flush();
    if (!DisableGameCodePatching())
        spdlog::warn("[payload] failed to restore one or more game code protections");

    spdlog::info("[payload] client init complete - releasing game");
    SignalInitDone();
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE aModule, DWORD aReason, LPVOID)
{
    if (aReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(aModule);

        // A init roda numa thread separada e NÃO é esperada aqui: RunTiltedInit
        // carrega módulos e instala hooks, o que precisa do loader lock que o
        // DllMain segura — esperar aqui seria deadlock.
        //
        // Não esperar é seguro porque a thread principal do jogo continua suspensa
        // até o launcher chamar ResumeThread, e o launcher só faz isso depois que a
        // injeção termina. A sincronização real é feita pelo launcher (ver
        // ExternalProcessLauncher::Run), não aqui.
        HANDLE hThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }

    return TRUE;
}
