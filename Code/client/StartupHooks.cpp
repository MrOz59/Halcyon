
#include <FunctionHook.hpp>
#include <mutex>
#include <TiltedCore/Initializer.hpp>

// Ativa o client quando o CRT do jogo chama uma destas funções durante o startup.
//
// Vive no client (e não no launcher) porque é usado pelos dois modos de
// carregamento: in-process, onde launcher e jogo compartilham o processo, e
// externo, onde o client é injetado no processo do jogo por STClientPayload.dll.
// No modo externo o símbolo launcher::InitClient não existe, então chamamos
// RunTiltedApp diretamente — é exatamente o que InitClient fazia.
extern void RunTiltedApp();

static std::once_flag s_initGuard;
static uint16_t(WINAPI* Real_crtGetShowWindowMode)() = nullptr;
static int(WINAPI* Real_ismbbled)(uint32_t) = nullptr;

void TP_GetStartupInfoW(LPSTARTUPINFOW apInfo) noexcept
{
    std::call_once(s_initGuard, []() { RunTiltedApp(); });
    GetStartupInfoW(apInfo);
}

int TP_ismbblead(uint32_t c)
{
    std::call_once(s_initGuard, []() { RunTiltedApp(); });
    return Real_ismbbled(c);
}

// Paliativo para a exceção de nomeação de thread do MSVC, mantido do upstream
// ("till we add SEH table support"): no modo in-process o jogo é auto-mapeado e
// suas unwind tables ficam invisíveis para o Wine, então deixar a exceção ser
// despachada quebra o unwind.
//
// No modo externo isto é desnecessário — o loader do Wine carrega a imagem e o
// unwind funciona, como medido em Code/linux_probe — mas manter o hook é
// inofensivo e preserva o comportamento no Windows.
void WINAPI TP_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, const ULONG_PTR* lpArguments)
{
    if (dwExceptionCode == 0x406D1388 && !IsDebuggerPresent())
        return; // thread naming

    RaiseException(dwExceptionCode, dwExceptionFlags, nNumberOfArguments, lpArguments);
}

void InstallStartHook()
{
    TP_HOOK_IAT2("Kernel32.dll", "GetStartupInfoW", TP_GetStartupInfoW);
    TP_HOOK_IAT2("Kernel32.dll", "RaiseException", TP_RaiseException);
};
