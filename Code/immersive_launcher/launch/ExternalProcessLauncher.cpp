// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include "ExternalProcessLauncher.h"
#include "Utils/Error.h"

#include <string>

#include <TiltedCore/Filesystem.hpp>
#include <spdlog/spdlog.h>

namespace launcher::launch
{
namespace
{
// A DLL que carrega o client dentro do processo do jogo. Fica ao lado do
// launcher, não na pasta do jogo.
constexpr const wchar_t* kClientPayloadName = L"STClientPayload.dll";

// Passa a configuração para o payload por variável de ambiente: o processo é
// criado com um bloco de ambiente herdado, então a DLL lê isso já no DllMain sem
// precisar de memória compartilhada nem IPC.
constexpr const wchar_t* kEnvGamePath = L"ST_GAME_PATH";
constexpr const wchar_t* kEnvExeVersion = L"ST_EXE_VERSION";

// O payload sinaliza este evento quando termina a init. Ele não pode fazer a init
// dentro do DllMain (deadlock no loader lock), então quem espera é o launcher,
// antes de resumir a thread principal do jogo.
constexpr const wchar_t* kInitDoneEventName = L"Local\\SkyrimTogether_ClientInitDone";

// Teto para a espera: se a init travar, é melhor seguir e deixar o jogo rodar sem
// o client do que congelar o launcher para sempre.
constexpr DWORD kInitTimeoutMs = 60000;
} // namespace

ExternalProcessLauncher::~ExternalProcessLauncher()
{
    Cleanup();
}

void ExternalProcessLauncher::Cleanup()
{
    if (m_mainThread)
    {
        CloseHandle(m_mainThread);
        m_mainThread = nullptr;
    }

    if (m_process)
    {
        CloseHandle(m_process);
        m_process = nullptr;
    }

    if (m_initDoneEvent)
    {
        CloseHandle(m_initDoneEvent);
        m_initDoneEvent = nullptr;
    }
}

bool ExternalProcessLauncher::InjectClient(const std::filesystem::path& acPayloadPath)
{
    const std::wstring pathStr = acPayloadPath.wstring();
    const SIZE_T byteCount = (pathStr.size() + 1) * sizeof(wchar_t);

    void* pRemoteMem = VirtualAllocEx(m_process, nullptr, byteCount, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem)
    {
        spdlog::error("[launch] VirtualAllocEx failed: {}", GetLastError());
        return false;
    }

    if (!WriteProcessMemory(m_process, pRemoteMem, pathStr.c_str(), byteCount, nullptr))
    {
        spdlog::error("[launch] WriteProcessMemory failed: {}", GetLastError());
        VirtualFreeEx(m_process, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    // kernel32 fica no mesmo endereço base em todos os processos da sessão, então
    // o ponteiro obtido aqui é válido no alvo.
    auto* pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!pLoadLibrary)
    {
        spdlog::error("[launch] could not resolve LoadLibraryW");
        VirtualFreeEx(m_process, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(m_process, nullptr, 0, pLoadLibrary, pRemoteMem, 0, nullptr);
    if (!hThread)
    {
        spdlog::error("[launch] CreateRemoteThread failed: {}", GetLastError());
        VirtualFreeEx(m_process, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    // O retorno do LoadLibraryW remoto vem truncado para 32 bits pelo exit code da
    // thread; serve para distinguir sucesso de falha, não como HMODULE.
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(m_process, pRemoteMem, 0, MEM_RELEASE);

    if (exitCode == 0)
    {
        spdlog::error("[launch] remote LoadLibraryW returned NULL - client payload not loaded");
        return false;
    }

    spdlog::info("[launch] client payload injected (truncated HMODULE=0x{:08x})", exitCode);
    return true;
}

bool ExternalProcessLauncher::Prepare(const LaunchRequest& acRequest)
{
    const auto payloadPath = TiltedPhoques::GetPath() / kClientPayloadName;

    if (!std::filesystem::exists(payloadPath))
    {
        spdlog::error("[launch] client payload not found at {}", payloadPath.string());
        Die(L"Client payload (STClientPayload.dll) is missing from the installation.");
        return false;
    }

    if (!std::filesystem::exists(acRequest.exePath))
    {
        spdlog::error("[launch] game executable not found at {}", acRequest.exePath.string());
        Die(L"Failed to find the game executable.");
        return false;
    }

    // O payload lê isto no DllMain. SetEnvironmentVariable no launcher basta
    // porque o filho herda o ambiente quando passamos nullptr em CreateProcessW.
    SetEnvironmentVariableW(kEnvGamePath, acRequest.gamePath.c_str());
    SetEnvironmentVariableW(kEnvExeVersion, std::wstring(acRequest.exeVersion.begin(), acRequest.exeVersion.end()).c_str());

    // Criado antes da injeção: o payload abre este evento pelo nome, então ele já
    // precisa existir quando a DLL entra no processo. Manual-reset para que o
    // sinal não se perca se a init terminar antes de começarmos a esperar.
    m_initDoneEvent = CreateEventW(nullptr, TRUE, FALSE, kInitDoneEventName);
    if (!m_initDoneEvent)
    {
        spdlog::error("[launch] CreateEventW failed: {}", GetLastError());
        Die(L"Failed to create the client synchronization event.");
        return false;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    // O working directory precisa ser o do jogo: o Skyrim resolve Data/ e as DLLs
    // dele relativo a isso. É o que InstallPathRouting fazia no modo in-process.
    const std::wstring workingDir = acRequest.gamePath.wstring();
    std::wstring commandLine = L"\"" + acRequest.exePath.wstring() + L"\"";

    // CreateProcess normal: é o loader do Wine que mapeia a imagem, que é
    // exatamente o ponto — ele popula as unwind tables que o auto-mapeamento não
    // consegue registrar.
    if (!CreateProcessW(
            acRequest.exePath.c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED,
            nullptr,
            workingDir.c_str(),
            &startupInfo,
            &processInfo))
    {
        spdlog::error("[launch] CreateProcessW failed: {}", GetLastError());
        Die(L"Failed to start the game process.");
        return false;
    }

    m_process = processInfo.hProcess;
    m_mainThread = processInfo.hThread;

    spdlog::info("[launch] game process created suspended (pid={})", processInfo.dwProcessId);

    // A injeção acontece com a thread principal ainda suspensa: é a janela que o
    // SKSE usa, antes de qualquer código do jogo rodar.
    if (!InjectClient(payloadPath))
    {
        TerminateProcess(m_process, 1);
        Cleanup();
        Die(L"Failed to inject the Skyrim Together client into the game.");
        return false;
    }

    return true;
}

bool ExternalProcessLauncher::Run()
{
    if (!m_process || !m_mainThread)
        return false;

    // Espera o client terminar a init antes de soltar o jogo. É esta espera — e
    // não um wait dentro do DllMain — que garante que os hooks estejam instalados
    // antes do entry point rodar.
    if (m_initDoneEvent)
    {
        spdlog::info("[launch] waiting for client init to complete");
        spdlog::default_logger()->flush();

        const DWORD waitResult = WaitForSingleObject(m_initDoneEvent, kInitTimeoutMs);
        if (waitResult == WAIT_TIMEOUT)
        {
            // Seguir mesmo assim: um jogo sem o client é melhor que um launcher
            // travado, e o log deixa a causa registrada.
            spdlog::error("[launch] client init timed out after {}ms - starting the game anyway", kInitTimeoutMs);
        }
        else if (waitResult != WAIT_OBJECT_0)
        {
            spdlog::error("[launch] wait for client init failed: {}", GetLastError());
        }
        else
        {
            spdlog::info("[launch] client init signalled");
        }
    }

    spdlog::info("[launch] resuming main thread - game entry point runs now");
    spdlog::default_logger()->flush();

    if (ResumeThread(m_mainThread) == static_cast<DWORD>(-1))
    {
        spdlog::error("[launch] ResumeThread failed: {}", GetLastError());
        TerminateProcess(m_process, 1);
        Cleanup();
        return false;
    }

    WaitForSingleObject(m_process, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(m_process, &exitCode);
    m_exitCode = exitCode;

    spdlog::info("[launch] game process exited with code {} (0x{:08x})", exitCode, exitCode);
    spdlog::default_logger()->flush();

    Cleanup();
    return true;
}

} // namespace launcher::launch
