#include <TiltedOnlinePCH.h>
#include "TiltedOnlineApp.h"
#include "LinuxDiag.h"

extern std::unique_ptr<TiltedOnlineApp> g_appInstance;

#include <GameVM.h>

#include <cstdio>

struct Main;
struct VMContext
{
    char pad[0x680];
    uint8_t inactive; // 0x680
};

TP_THIS_FUNCTION(TVMUpdate, int, VMContext, float);
TP_THIS_FUNCTION(TMainLoop, short, Main);
TP_THIS_FUNCTION(TVMDestructor, uintptr_t, void);

static TVMUpdate* VMUpdate = nullptr;
static TMainLoop* MainLoop = nullptr;
static TVMDestructor* VMDestructor = nullptr;

int TP_MAKE_THISCALL(HookVMUpdate, VMContext, float a2)
{
    // Diagnóstico: registra a primeira passagem e o estado do trampolim de detour
    // (VMUpdate). Se MH_CreateHook falhou silenciosamente sob Wine, VMUpdate fica
    // inválido e o ThisCall abaixo salta para lixo — hipótese para o 0x80000003.
    static bool s_firstUpdate = true;
    if (s_firstUpdate)
    {
        s_firstUpdate = false;
        char buf[128];
        _snprintf_s(buf, _TRUNCATE, "HookVMUpdate first call, VMUpdate detour=0x%llx", reinterpret_cast<unsigned long long>(VMUpdate));
        LinuxDiagStep(buf);
    }

    if (apThis->inactive == 0)
        g_appInstance->Update();

    static bool s_firstThisCall = true;
    if (s_firstThisCall)
    {
        s_firstThisCall = false;
        LinuxDiagStep("HookVMUpdate: Update() returned, calling original VMUpdate");
    }

    const auto result = TiltedPhoques::ThisCall(VMUpdate, apThis, a2);

    static bool s_firstDone = true;
    if (s_firstDone)
    {
        s_firstDone = false;
        LinuxDiagStep("HookVMUpdate: original VMUpdate returned OK");
    }

    return result;
}

short TP_MAKE_THISCALL(HookMainLoop, Main)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(MainLoop, apThis);
}

uintptr_t TP_MAKE_THISCALL(HookVMDestructor, void)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(VMDestructor, apThis);
}

static TiltedPhoques::Initializer s_mainHooks(
    []()
    {
        POINTER_SKYRIMSE(TMainLoop, cMainLoop, 36564);
        POINTER_SKYRIMSE(TVMUpdate, cVMUpdate, 53926);
        POINTER_SKYRIMSE(TVMDestructor, cVMDestructor, 40412);

        VMUpdate = cVMUpdate.Get();
        MainLoop = cMainLoop.Get();
        VMDestructor = cVMDestructor.Get();

        TP_HOOK(&VMUpdate, HookVMUpdate);
        TP_HOOK(&MainLoop, HookMainLoop);
        TP_HOOK(&VMDestructor, HookVMDestructor);
    });

