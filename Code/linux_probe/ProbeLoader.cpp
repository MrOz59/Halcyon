// linux_probe: loader de diagnóstico.
//
// Cria o SkyrimSE.exe como processo REAL e suspenso, injeta o payload antes do
// entry point, e resume. O ponto do teste é justamente NÃO auto-mapear o PE: se o
// loader do Wine carregar a imagem, ele popula as estruturas de unwind ele mesmo e
// o RtlVirtualUnwind2 passa a funcionar dentro do código do jogo.
//
// Ver README.md para a hipótese completa e como interpretar o resultado.

#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace
{

std::filesystem::path GetSelfDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return std::filesystem::current_path();

    return std::filesystem::path(buffer).parent_path();
}

FILE* g_pLog = nullptr;

void LogOpen()
{
    const auto logPath = GetSelfDirectory() / L"probe_loader.log";
    _wfopen_s(&g_pLog, logPath.c_str(), L"w");
}

void Log(const char* apFormat, ...)
{
    va_list args;

    va_start(args, apFormat);
    vfprintf(stdout, apFormat, args);
    va_end(args);
    fputc('\n', stdout);
    fflush(stdout);

    if (g_pLog)
    {
        va_start(args, apFormat);
        vfprintf(g_pLog, apFormat, args);
        va_end(args);
        fputc('\n', g_pLog);
        fflush(g_pLog);
    }
}

// Injeta a DLL no processo alvo: escreve o caminho na memória dele e roda
// LoadLibraryW lá via CreateRemoteThread. kernel32 está no mesmo endereço base em
// todos os processos da sessão, então o ponteiro de LoadLibraryW obtido aqui é
// válido no alvo.
bool InjectPayload(HANDLE aProcess, const std::filesystem::path& acPayloadPath)
{
    const std::wstring pathStr = acPayloadPath.wstring();
    const SIZE_T byteCount = (pathStr.size() + 1) * sizeof(wchar_t);

    void* pRemoteMem = VirtualAllocEx(aProcess, nullptr, byteCount, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem)
    {
        Log("[loader] VirtualAllocEx failed: %lu", GetLastError());
        return false;
    }

    if (!WriteProcessMemory(aProcess, pRemoteMem, pathStr.c_str(), byteCount, nullptr))
    {
        Log("[loader] WriteProcessMemory failed: %lu", GetLastError());
        VirtualFreeEx(aProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    auto* pLoadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!pLoadLibrary)
    {
        Log("[loader] could not resolve LoadLibraryW");
        VirtualFreeEx(aProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(aProcess, nullptr, 0, pLoadLibrary, pRemoteMem, 0, nullptr);
    if (!hThread)
    {
        Log("[loader] CreateRemoteThread failed: %lu", GetLastError());
        VirtualFreeEx(aProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    // O valor de retorno do LoadLibraryW remoto é truncado para 32 bits pelo
    // exit code da thread. Serve para distinguir sucesso de falha, não como HMODULE.
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(aProcess, pRemoteMem, 0, MEM_RELEASE);

    if (exitCode == 0)
    {
        Log("[loader] remote LoadLibraryW returned NULL - payload not loaded");
        return false;
    }

    Log("[loader] payload loaded in target (truncated HMODULE=0x%08lx)", exitCode);
    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    LogOpen();

    Log("[loader] linux_probe loader starting");

    if (argc < 2)
    {
        Log("[loader] usage: probe_loader.exe <path-to-SkyrimSE.exe>");
        return 1;
    }

    const std::filesystem::path gamePath = argv[1];
    const std::filesystem::path payloadPath = GetSelfDirectory() / L"probe_payload.dll";

    Log("[loader] game    : %ls", gamePath.c_str());
    Log("[loader] payload : %ls", payloadPath.c_str());

    if (!std::filesystem::exists(gamePath))
    {
        Log("[loader] game executable not found");
        return 2;
    }

    if (!std::filesystem::exists(payloadPath))
    {
        Log("[loader] probe_payload.dll not found next to the loader");
        return 3;
    }

    // O working directory precisa ser o do jogo: o Skyrim resolve Data/ e as DLLs
    // dele relativo a isso.
    const std::wstring workingDir = gamePath.parent_path().wstring();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};

    // CreateProcess normal: é o loader do Wine que mapeia a imagem, que é
    // exatamente a variável sendo testada.
    std::wstring commandLine = L"\"" + gamePath.wstring() + L"\"";

    if (!CreateProcessW(
            gamePath.c_str(),
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
        Log("[loader] CreateProcessW failed: %lu", GetLastError());
        return 4;
    }

    Log("[loader] process created suspended (pid=%lu)", processInfo.dwProcessId);

    if (!InjectPayload(processInfo.hProcess, payloadPath))
    {
        Log("[loader] injection failed; terminating target");
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return 5;
    }

    Log("[loader] resuming main thread - game entry point runs now");

    if (ResumeThread(processInfo.hThread) == static_cast<DWORD>(-1))
    {
        Log("[loader] ResumeThread failed: %lu", GetLastError());
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return 6;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    // Exit code 136 (SIGFPE) é a assinatura do crash de unwind que este teste
    // investiga; ver a tabela de interpretação no README.
    Log("[loader] game process exited with code %lu (0x%08lx)", exitCode, exitCode);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    if (g_pLog)
        fclose(g_pLog);

    return 0;
}
