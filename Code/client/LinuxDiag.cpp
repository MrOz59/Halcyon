#include <TiltedOnlinePCH.h>

#include "LinuxDiag.h"
#include "Platform.h"

#include <cstdio>
#include <filesystem>

void LinuxDiagStep(const char* apStep)
{
    // This synchronous crash trail exists for the Proton path only. Keeping it
    // disabled on native Windows preserves the upstream runtime behavior while
    // retaining the diagnostics that are still useful for Wine regressions.
    if (!IsRunningUnderWine())
        return;

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
    const int n = _snprintf_s(line, _TRUNCATE, "[diag] %s\r\n", apStep);
    DWORD written = 0;
    WriteFile(s_hFile, line, static_cast<DWORD>(n), &written, nullptr);
    FlushFileBuffers(s_hFile);
}
