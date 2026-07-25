// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include "ExternalProcessLauncher.h"
#include "Utils/Error.h"
#include "steam/SteamCeg.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <TlHelp32.h>
#include <winternl.h>

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

void* GetRemoteImageBase(HANDLE aProcess)
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return nullptr;

    using TNtQueryInformationProcess = NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    const auto pNtQueryInformationProcess = reinterpret_cast<TNtQueryInformationProcess>(GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!pNtQueryInformationProcess)
        return nullptr;

    PROCESS_BASIC_INFORMATION processInfo{};
    if (pNtQueryInformationProcess(aProcess, ProcessBasicInformation, &processInfo, sizeof(processInfo), nullptr) < 0 || !processInfo.PebBaseAddress)
    {
        return nullptr;
    }

    // ImageBaseAddress fica em 0x10 no PEB x64. Usar um offset explícito evita
    // depender dos nomes dos campos reservados, que diferem entre o SDK e Wine.
    static_assert(sizeof(void*) == 8);
    constexpr size_t kPebImageBaseOffset = 0x10;
    const auto imageBaseField = reinterpret_cast<const uint8_t*>(processInfo.PebBaseAddress) + kPebImageBaseOffset;

    void* pImageBase = nullptr;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(aProcess, imageBaseField, &pImageBase, sizeof(pImageBase), &bytesRead) || bytesRead != sizeof(pImageBase))
        return nullptr;

    return pImageBase;
}

bool WriteRemoteExecutable(HANDLE aProcess, void* apAddress, const void* apData, size_t aSize)
{
    DWORD oldProtection = 0;
    if (!VirtualProtectEx(aProcess, apAddress, aSize, PAGE_EXECUTE_READWRITE, &oldProtection))
        return false;

    SIZE_T bytesWritten = 0;
    const bool writeSucceeded = WriteProcessMemory(aProcess, apAddress, apData, aSize, &bytesWritten) != FALSE && bytesWritten == aSize;
    const bool flushSucceeded = writeSucceeded && FlushInstructionCache(aProcess, apAddress, aSize) != FALSE;

    DWORD ignoredProtection = 0;
    const bool restoreSucceeded = VirtualProtectEx(aProcess, apAddress, aSize, oldProtection, &ignoredProtection) != FALSE;

    return writeSucceeded && flushSucceeded && restoreSucceeded;
}

bool ValidateRemoteGameImage(HANDLE aProcess, uintptr_t aImageBase, const steam::CEGImageInfo& acInfo)
{
    IMAGE_DOS_HEADER dosHeader{};
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(aProcess, reinterpret_cast<const void*>(aImageBase), &dosHeader, sizeof(dosHeader), &bytesRead) || bytesRead != sizeof(dosHeader) ||
        dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew < 0 || dosHeader.e_lfanew > 1024 * 1024)
    {
        return false;
    }

    if (aImageBase > std::numeric_limits<uintptr_t>::max() - static_cast<uintptr_t>(dosHeader.e_lfanew))
        return false;

    IMAGE_NT_HEADERS64 ntHeaders{};
    bytesRead = 0;
    const uintptr_t ntHeadersAddress = aImageBase + static_cast<uintptr_t>(dosHeader.e_lfanew);
    if (!ReadProcessMemory(aProcess, reinterpret_cast<const void*>(ntHeadersAddress), &ntHeaders, sizeof(ntHeaders), &bytesRead) || bytesRead != sizeof(ntHeaders))
    {
        return false;
    }

    return ntHeaders.Signature == IMAGE_NT_SIGNATURE && ntHeaders.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 && ntHeaders.FileHeader.NumberOfSections != 0 &&
           ntHeaders.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64) && ntHeaders.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
           ntHeaders.OptionalHeader.ImageBase == acInfo.preferredImageBase && ntHeaders.OptionalHeader.SizeOfImage == acInfo.imageSize &&
           ntHeaders.OptionalHeader.AddressOfEntryPoint == acInfo.protectedEntryPointRva;
}

bool FindRemoteModule(HANDLE aProcess, const std::filesystem::path& acModulePath, uintptr_t& aBase, size_t& aSize)
{
    const DWORD processId = GetProcessId(aProcess);
    if (processId == 0)
        return false;

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    const std::wstring expectedName = acModulePath.filename().wstring();
    MODULEENTRY32W moduleEntry{};
    moduleEntry.dwSize = sizeof(moduleEntry);

    bool found = false;
    if (Module32FirstW(snapshot, &moduleEntry))
    {
        do
        {
            if (_wcsicmp(moduleEntry.szModule, expectedName.c_str()) == 0)
            {
                aBase = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                aSize = moduleEntry.modBaseSize;
                found = aSize != 0;
                break;
            }
        } while (Module32NextW(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return found;
}
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

    // O retorno do LoadLibraryW remoto vem truncado para 32 bits pelo exit code
    // da thread. Ele serve apenas para detectar NULL; a base completa é obtida
    // pela lista de módulos do processo logo abaixo.
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(m_process, pRemoteMem, 0, MEM_RELEASE);

    if (exitCode == 0)
    {
        spdlog::error("[launch] remote LoadLibraryW returned NULL - client payload not loaded");
        return false;
    }

    uintptr_t payloadBase = 0;
    size_t payloadSize = 0;
    if (!FindRemoteModule(m_process, acPayloadPath, payloadBase, payloadSize))
    {
        spdlog::error("[launch] payload loaded but its full module address could not be resolved");
        return false;
    }

    // O payload pode cair em qualquer região do espaço de endereços. Não há mais
    // validação de alcance: os hooks rel32 do client saltam para um relay absoluto
    // no pool RIP (perto do jogo), então a base da DLL é irrelevante.
    spdlog::info("[launch] client payload loaded at 0x{:x} (size=0x{:x})", payloadBase, payloadSize);
    spdlog::info("[launch] payload placement accepted - direct rel32 hooks use near relays");
    return true;
}

bool ExternalProcessLauncher::PrepareCegImage(const std::filesystem::path& acExePath)
{
    auto content = TiltedPhoques::LoadFile(acExePath);
    if (content.empty())
    {
        spdlog::error("[launch] failed to read executable for Steam CEG preparation");
        return false;
    }

    steam::CEGImageInfo cegInfo{};
    const auto cegResult = steam::DecryptCEGInPlace(reinterpret_cast<uint8_t*>(content.data()), content.size(), cegInfo);

    if (cegResult == steam::CEGDecryptResult::kNotProtected)
    {
        spdlog::info("[launch] executable is not protected by Steam CEG");
        return true;
    }

    if (cegResult != steam::CEGDecryptResult::kDecrypted)
    {
        spdlog::error("[launch] unsupported or invalid Steam CEG image - refusing to patch encrypted game code");
        return false;
    }

    void* pRemoteImageBase = GetRemoteImageBase(m_process);
    if (!pRemoteImageBase)
    {
        spdlog::error("[launch] could not resolve remote image base");
        return false;
    }

    const uintptr_t remoteImageBase = reinterpret_cast<uintptr_t>(pRemoteImageBase);
    if (!ValidateRemoteGameImage(m_process, remoteImageBase, cegInfo))
    {
        spdlog::error("[launch] remote CEG image headers do not match the selected Skyrim executable");
        return false;
    }

    if (remoteImageBase != cegInfo.preferredImageBase)
    {
        uint32_t appliedRelocations = 0;
        const auto relocationResult = steam::RelocateCEGTextInPlace(reinterpret_cast<uint8_t*>(content.data()), content.size(), cegInfo, remoteImageBase, appliedRelocations);
        if (relocationResult == steam::CEGRelocateResult::kUnsupportedRelocation)
        {
            spdlog::error("[launch] CEG .text contains an unsupported PE relocation type");
            return false;
        }
        if (relocationResult != steam::CEGRelocateResult::kRelocated)
        {
            spdlog::error("[launch] failed to validate or relocate decrypted CEG code for image base 0x{:x}", remoteImageBase);
            return false;
        }

        const uint64_t relocationDelta = remoteImageBase - cegInfo.preferredImageBase;
        spdlog::info(
            "[launch] CEG image relocated from 0x{:x} to 0x{:x}; applied {} .text fixups (delta 0x{:x})", cegInfo.preferredImageBase, remoteImageBase, appliedRelocations,
            relocationDelta);
    }

    const uintptr_t remoteTextAddress = remoteImageBase + cegInfo.textRva;
    const auto* pDecryptedText = reinterpret_cast<const uint8_t*>(content.data()) + cegInfo.textFileOffset;

    spdlog::info("[launch] Steam CEG detected - restoring {} decrypted bytes at 0x{:x}", cegInfo.textSize, remoteTextAddress);

    if (!WriteRemoteExecutable(m_process, reinterpret_cast<void*>(remoteTextAddress), pDecryptedText, cegInfo.textSize))
    {
        spdlog::error("[launch] failed to install decrypted Steam CEG text: {}", GetLastError());
        return false;
    }

    // Verify through an independent read before changing the protected entry
    // point. The game remains suspended if Wine reports a successful write but
    // the remote code does not match the decrypted image.
    std::array<uint8_t, 16> remoteText{};
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(m_process, reinterpret_cast<const void*>(remoteTextAddress), remoteText.data(), remoteText.size(), &bytesRead) || bytesRead != remoteText.size() ||
        std::memcmp(remoteText.data(), pDecryptedText, remoteText.size()) != 0)
    {
        spdlog::error("[launch] Steam CEG text verification failed");
        return false;
    }

    // O thread suspenso já recebeu o entry point protegido do loader. Em vez de
    // depender do layout interno do CONTEXT/RtlUserThreadStart, substituímos
    // somente o início do stub por um JMP relativo ao entry point original.
    const uintptr_t protectedEntryPoint = remoteImageBase + cegInfo.protectedEntryPointRva;
    const uintptr_t originalEntryPoint = remoteImageBase + cegInfo.originalEntryPointRva;
    const int64_t displacement64 = static_cast<int64_t>(originalEntryPoint) - static_cast<int64_t>(protectedEntryPoint + 5);
    if (displacement64 < std::numeric_limits<int32_t>::min() || displacement64 > std::numeric_limits<int32_t>::max())
    {
        spdlog::error("[launch] Steam CEG entry point is out of range");
        return false;
    }

    std::array<uint8_t, 5> jump{0xE9, 0, 0, 0, 0};
    const int32_t displacement = static_cast<int32_t>(displacement64);
    std::memcpy(jump.data() + 1, &displacement, sizeof(displacement));

    if (!WriteRemoteExecutable(m_process, reinterpret_cast<void*>(protectedEntryPoint), jump.data(), jump.size()))
    {
        spdlog::error("[launch] failed to bypass Steam CEG stub: {}", GetLastError());
        return false;
    }

    std::array<uint8_t, 5> remoteJump{};
    bytesRead = 0;
    if (!ReadProcessMemory(m_process, reinterpret_cast<const void*>(protectedEntryPoint), remoteJump.data(), remoteJump.size(), &bytesRead) || bytesRead != remoteJump.size() ||
        remoteJump != jump)
    {
        spdlog::error("[launch] Steam CEG entry point verification failed");
        return false;
    }

    spdlog::info("[launch] Steam CEG prepared - protected EP 0x{:x} now enters original EP 0x{:x}", protectedEntryPoint, originalEntryPoint);
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

    // O loader do Wine mapeia a imagem e registra unwind/TLS/imports. O Steam CEG
    // ainda é tratado abaixo, antes de qualquer hook tocar no .text.
    if (!CreateProcessW(acRequest.exePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, workingDir.c_str(), &startupInfo, &processInfo))
    {
        spdlog::error("[launch] CreateProcessW failed: {}", GetLastError());
        Die(L"Failed to start the game process.");
        return false;
    }

    m_process = processInfo.hProcess;
    m_mainThread = processInfo.hThread;

    spdlog::info("[launch] game process created suspended (pid={})", processInfo.dwProcessId);

    if (!PrepareCegImage(acRequest.exePath))
    {
        TerminateProcess(m_process, 1);
        Cleanup();
        Die(L"Failed to prepare the Steam-protected game image safely.");
        return false;
    }

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
