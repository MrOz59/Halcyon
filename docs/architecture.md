# Architecture (as-is)

> Documento da **Fase 0 — Engenharia Reversa** do roadmap de portabilidade para Linux.
> Descreve como o projeto funciona **hoje**, antes de qualquer alteração. É a base
> de referência para as fases seguintes.

## Visão geral

TiltedEvolution (Skyrim Together Reborn / Tilted Online) é um framework de
multiplayer para Skyrim Special Edition. Ele se divide em dois mundos bem
distintos do ponto de vista de portabilidade:

| Componente        | Roda onde                          | Estado no Linux |
|-------------------|------------------------------------|-----------------|
| **Server**        | Processo dedicado (headless)       | ✅ Já compila e roda nativo (Docker/xmake → `libSTServer.so` + `SkyrimTogetherServer`) |
| **Client + Launcher** | Injetado dentro do `SkyrimSE.exe` | ❌ Windows-only (CEF, usvfs, COM, LoadLibrary, ntdll) |
| **tp_process**    | Processo auxiliar do overlay (CEF) | ❌ Windows-only |

O corte de plataforma é explícito em [`Code/xmake.lua`](../Code/xmake.lua):

```lua
if is_plat("windows") then
    includes("client")
    includes("immersive_elf")
    includes("immersive_launcher")
    includes("tp_process")
end
-- server, common, encoding, components, base... sempre incluídos
```

## Árvore de código relevante

- `Code/immersive_launcher/` — Launcher/updater. Ponto de entrada do lado do jogo.
- `Code/client/` — Código do client (hooks no jogo, serviços de sync, overlay).
- `Code/tp_process/` — Worker do CEF (Chromium) que renderiza a overlay/UI.
- `Code/skyrim_ui/` — UI em TypeScript (renderizada dentro do CEF).
- `Code/server/`, `Code/server_runner/` — Servidor dedicado.
- `Code/common/`, `Code/encoding/`, `Code/components/`, `Code/base/` — Código compartilhado.
- `Code/immersive_elf/` — Loader/injeção (early load DLL).

## Fluxo real de inicialização (client)

O diagrama do roadmap sugere `Launcher → SkyrimTogether.exe → SkyrimSE.exe → DLLs → tp_process`.
A **realidade do código** é mais acoplada: **não há `CreateProcess` do jogo**. O launcher
carrega o executável do jogo **dentro do próprio processo** via um loader manual de PE
(`ExeLoader`) e salta para o entry point. Launcher, jogo e client vivem no **mesmo processo**.

```
SkyrimTogether.exe (o launcher — Code/immersive_launcher)
  │
  │  Main.cpp:main()
  │    ├── script_extender::SEMemoryBlock  (reserva zona de memória cedo)
  │    ├── PreloadSystemDlls()             (dinput8/dsound/xinput/version...)
  │    ├── CoreStubsInit()
  │    └── launcher::StartUp(argc, argv)
  │
  ▼  Launcher.cpp:StartUp()
  ├── HandleArguments()          (-r, --exePath)
  ├── EarlyInstallSucceeded()    (checa EarlyLoad.dll)
  ├── oobe::ReportModCompatabilityStatus()  (DX11 / versão do SO)
  ├── oobe::SelectInstall()      (descobre o caminho do Skyrim)
  ├── loader::InstallPathRouting(gamePath)
  ├── steam::Load(gamePath)      (integração Steam)
  ├── LoadProgram(LC):
  │     ├── LoadFile(exePath)                    (lê SkyrimSE.exe do disco)
  │     ├── QueryFileVersion()                   (versão do EXE)
  │     └── ExeLoader.Load(content) ─────────────► MAPEIA O JOGO NO PROCESSO
  │           └── GetEntryPoint() → LC.gameMain
  ├── InstallStartHook()         (client — hook de início)
  ├── RunTiltedInit(gamePath, Version):
  │     ├── VersionDb::Load()    (Address Library — SKSE)
  │     ├── new TiltedOnlineApp()
  │     └── InstallHooks2() + TP_HOOK_COMMIT
  └── LC.gameMain()              ◄── entra no jogo; só retorna quando o jogo morre
        │
        │  em algum momento durante o boot do jogo, via hook:
        ▼
      RunTiltedApp() → g_appInstance->BeginMain()   (loop do client)
        │
        ▼
      OverlayService / OverlayClient  ──► CEF  ──► lança tp_process.exe (overlay)
```

Símbolos que costuram launcher ↔ client (definidos no client, chamados no launcher),
ver [`Launcher.cpp`](../Code/immersive_launcher/Launcher.cpp):

```cpp
extern void InstallStartHook();
extern void RunTiltedApp();
extern void RunTiltedInit(const std::filesystem::path&, const TiltedPhoques::String&);
```

## Onde o CEF é usado

O CEF (Chromium Embedded Framework) é o backend da overlay/UI in-game:

- `Code/tp_process/main.cpp` — processo auxiliar do CEF; `WinMain` chama
  `TiltedPhoques::UIMain(...)` com um `ProcessHandler`. É o "render/util process" do CEF.
- `Code/client/Services/OverlayService.*` e `OverlayClient.*` — lado do client que
  fala com o CEF e renderiza a overlay dentro do jogo.
- `Code/skyrim_ui/` — a UI em si (TypeScript/Angular), carregada dentro do CEF.

A dependência do CEF é o principal bloqueador para um launcher desacoplado (Fase 4 do roadmap).

## Pontos Windows-específicos (inventário para o port)

### immersive_launcher
- `Main.cpp`: `Windows.h`, `combaseapi.h`, `LoadLibraryW`, `GetSystemDirectoryW`,
  `__declspec(dllexport)`, preload de DLLs de sistema, COM (`ComScope`).
- `Launcher.cpp`: `ExeLoader` (mapeamento manual de PE — **intrinsecamente Windows/PE**),
  `GetModuleHandleW`/`GetProcAddress`, `GetAsyncKeyState`, `SetLastError`,
  `Die()` com `TaskDialog` (Win32), `steam::Load`, `Registry`.
- `loader/` — `ExeLoader`, `PathRerouting` (rerota chamadas de path do jogo).
- `usvfs/` — Virtual File System em modo usuário (injeção `usvfs_64.dll`).
- `script_extender/` — reserva de memória para SKSE.
- `ntdll_x64.lib` — link direto contra ntdll.
- Recursos Win32: `launcher.rc`, `launcher.aps`.

### client
- `main.cpp`: `Windows.h`, `Commctrl.h`, `TaskDialog`, `ShellExecuteW`, `HICON`.
- `CrashHandler.*` — crash handler baseado em SEH/Win32.
- `imgui_impl_win32.cpp`, `imgui_impl_dx11.cpp` — backend gráfico Win32/DX11.
- Milhares de hooks contra endereços do `SkyrimSE.exe` (Address Library / SKSE).

### tp_process
- `main.cpp`: `WinMain`, `HINSTANCE`. Processo CEF Windows-only.
- `process.manifest`, `process.rc`.

## Implicação central para o port (resumo)

Como **o client é código injetado no processo do jogo Windows** (`SkyrimSE.exe`), a
estratégia realista no Linux **não é** recompilar o client como binário ELF nativo — é
rodar o conjunto **launcher+jogo+client sob Proton/Wine**, onde a ABI Windows já existe.
O trabalho de "Linux" concentra-se em:

1. **Servidor nativo** — já viável; consolidar build/CI (Fases 6–7).
2. **Launcher rodar bem dentro do prefixo Proton** (Fase 5 — "funciona no Proton").
3. **Abstrair o que não precisa de Windows** (config, logs, seleção de path, abrir
   browser, diálogos) atrás de uma `IPlatform` (Fase 2), para que partes possam virar
   ferramentas Linux nativas (ex.: um launcher/CLI que prepara o prefixo).

Ver [`linux.md`](linux.md) para a estratégia detalhada.
