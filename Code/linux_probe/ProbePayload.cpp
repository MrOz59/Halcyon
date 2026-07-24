// linux_probe: payload de diagnóstico.
//
// Observador puro. Instala um vectored exception handler que loga toda exceção
// despachada e SEMPRE devolve EXCEPTION_CONTINUE_SEARCH, sem alterar o
// comportamento do jogo. Nada do STR é carregado aqui: o teste isola a variável
// "modo de carregamento" e nada mais.
//
// O que se procura no log:
//   - 0x406D1388 (EXCEPTION_WINE_NAME_THREAD) sendo despachada e o jogo seguindo
//     em frente => o unwind funciona, hipótese confirmada.
//   - a mesma exceção seguida de cascata/silêncio => o modo de carregamento não
//     era a causa.
//
// Ver README.md para a tabela de interpretação.

#include <Windows.h>

#include <cstdio>
#include <filesystem>

namespace
{

constexpr DWORD kWineNameThread = 0x406D1388;

FILE* g_pLog = nullptr;
CRITICAL_SECTION g_logLock;
LONG g_exceptionCount = 0;
PVOID g_pHandler = nullptr;

void Log(const char* apFormat, ...)
{
    EnterCriticalSection(&g_logLock);

    va_list args;
    va_start(args, apFormat);
    if (g_pLog)
    {
        vfprintf(g_pLog, apFormat, args);
        fputc('\n', g_pLog);
        fflush(g_pLog);
    }
    va_end(args);

    LeaveCriticalSection(&g_logLock);
}

const char* ExceptionName(DWORD aCode)
{
    switch (aCode)
    {
    case kWineNameThread: return "MSVC thread-name (EXCEPTION_WINE_NAME_THREAD)";
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
    case 0xE06D7363: return "C++ exception";
    default: return "other";
    }
}

// Resolve o módulo dono de um endereço. É isso que diz se o unwind está entrando
// na imagem do jogo (base 0x140000000 quando auto-mapeado) ou num módulo que o
// loader do Wine conhece de verdade.
void DescribeAddress(void* apAddress, char* apBuffer, size_t aBufferSize)
{
    HMODULE hModule = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(apAddress),
            &hModule) &&
        hModule)
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

LONG CALLBACK ProbeVectoredHandler(EXCEPTION_POINTERS* apInfo)
{
    const auto* pRecord = apInfo->ExceptionRecord;
    const LONG index = InterlockedIncrement(&g_exceptionCount);

    char addressDesc[512]{};
    DescribeAddress(pRecord->ExceptionAddress, addressDesc, sizeof(addressDesc));

    char ripDesc[512]{};
    DescribeAddress(reinterpret_cast<void*>(apInfo->ContextRecord->Rip), ripDesc, sizeof(ripDesc));

    Log("[#%ld] tid=%lu code=0x%08lx (%s) flags=0x%lx",
        index,
        GetCurrentThreadId(),
        pRecord->ExceptionCode,
        ExceptionName(pRecord->ExceptionCode),
        pRecord->ExceptionFlags);
    Log("       addr = %p  %s", pRecord->ExceptionAddress, addressDesc);
    Log("       rip  = 0x%llx  %s", static_cast<unsigned long long>(apInfo->ContextRecord->Rip), ripDesc);
    Log("       rsp  = 0x%llx", static_cast<unsigned long long>(apInfo->ContextRecord->Rsp));

    if (pRecord->ExceptionCode == kWineNameThread && pRecord->NumberParameters >= 2)
    {
        // Layout do THREADNAME_INFO legado do MSVC: [0]=type, [1]=ponteiro p/ nome,
        // [2]=thread id. O ponteiro é do processo do jogo, então lê-lo pode falhar
        // se a memória já saiu de escopo - daí o guard.
        const auto* pName = reinterpret_cast<const char*>(pRecord->ExceptionInformation[1]);
        if (pName && !IsBadReadPtr(pName, 1))
            Log("       thread name = \"%s\"", pName);
    }

    // Observador: nunca altera o fluxo. Consumir a exceção aqui mascararia o
    // resultado do teste.
    return EXCEPTION_CONTINUE_SEARCH;
}

DWORD WINAPI InitThread(LPVOID)
{
    // Handler registrado como PRIMEIRO da cadeia (1), para ver a exceção antes de
    // qualquer handler do jogo.
    g_pHandler = AddVectoredExceptionHandler(1, ProbeVectoredHandler);
    Log("[payload] vectored handler installed: %s", g_pHandler ? "ok" : "FAILED");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE aModule, DWORD aReason, LPVOID)
{
    if (aReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(aModule);

        InitializeCriticalSection(&g_logLock);

        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(aModule, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).parent_path() / L"probe_payload.log";
        _wfopen_s(&g_pLog, logPath.c_str(), L"w");

        Log("[payload] attached to pid=%lu", GetCurrentProcessId());

        HMODULE hExe = GetModuleHandleW(nullptr);
        Log("[payload] target image base = 0x%llx", reinterpret_cast<unsigned long long>(hExe));

        // DllMain roda sob o loader lock; AddVectoredExceptionHandler toca em
        // estruturas do ntdll, então registra fora dele.
        HANDLE hThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
        else
            Log("[payload] CreateThread for init failed: %lu", GetLastError());
    }
    else if (aReason == DLL_PROCESS_DETACH)
    {
        Log("[payload] detaching - %ld exceptions observed", g_exceptionCount);

        if (g_pHandler)
            RemoveVectoredExceptionHandler(g_pHandler);

        if (g_pLog)
            fclose(g_pLog);
    }

    return TRUE;
}
