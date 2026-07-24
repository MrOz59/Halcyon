#pragma once

#include <windows.h>

// Detecção de ambiente Wine/Proton no lado do client. wine_get_version só existe
// no ntdll do Wine — é a checagem canônica documentada pelo próprio projeto Wine.
// Usado para desabilitar caminhos que não funcionam sob Wine (ex.: overlay CEF,
// cujo CefInitialize bate um CHECK do Chromium sob Wine — ver docs/cef-proton.md).
inline bool IsRunningUnderWine() noexcept
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return false;

    return GetProcAddress(ntdll, "wine_get_version") != nullptr;
}
