# Architecture (original upstream path)

> Phase 0 reverse-engineering document for the Linux portability roadmap. It
> describes how the upstream project worked **before the porting changes** and serves
> as a reference for the architecture implemented by this fork.

## Overview

TiltedEvolution (Skyrim Together Reborn / Tilted Online) is a multiplayer framework
for Skyrim Special Edition. From a portability perspective, it is divided into two
very different areas:

| Component | Runtime location | Original Linux status |
| --- | --- | --- |
| **Server** | Dedicated headless process | ✅ Already builds and runs natively (Docker/xmake → `libSTServer.so` + `SkyrimTogetherServer`) |
| **Client + Launcher** | Injected into `SkyrimSE.exe` | ❌ Windows-only (CEF, usvfs, COM, LoadLibrary, ntdll) |
| **tp_process** | CEF overlay helper process | ❌ Windows-only |

The platform split is explicit in [`Code/xmake.lua`](../Code/xmake.lua):

```lua
if is_plat("windows") then
    includes("client")
    includes("immersive_elf")
    includes("immersive_launcher")
    includes("tp_process")
end
-- server, common, encoding, components, base... are always included
```

## Relevant source tree

- `Code/immersive_launcher/` — launcher/updater and game-side entry point.
- `Code/client/` — client code: game hooks, synchronization services, and overlay.
- `Code/tp_process/` — CEF/Chromium worker that renders the overlay/UI.
- `Code/skyrim_ui/` — TypeScript UI rendered inside CEF.
- `Code/server/`, `Code/server_runner/` — dedicated server.
- `Code/common/`, `Code/encoding/`, `Code/components/`, `Code/base/` — shared code.
- `Code/immersive_elf/` — early loader/injection DLL.

## Original client startup flow

The roadmap diagram suggested
`Launcher → SkyrimTogether.exe → SkyrimSE.exe → DLLs → tp_process`. The original code
was more tightly coupled: it did **not** use `CreateProcess` for the game. The launcher
loaded the game executable **inside its own process** through a manual PE loader
(`ExeLoader`) and jumped to its entry point. Launcher, game, and client all lived in
the **same process**.

```text
SkyrimTogether.exe (launcher — Code/immersive_launcher)
  │
  │  Main.cpp:main()
  │    ├── script_extender::SEMemoryBlock  (reserves memory early)
  │    ├── PreloadSystemDlls()             (dinput8/dsound/xinput/version...)
  │    ├── CoreStubsInit()
  │    └── launcher::StartUp(argc, argv)
  │
  ▼  Launcher.cpp:StartUp()
  ├── HandleArguments()          (-r, --exePath)
  ├── EarlyInstallSucceeded()    (checks EarlyLoad.dll)
  ├── oobe::ReportModCompatabilityStatus()  (DX11 / OS version)
  ├── oobe::SelectInstall()      (locates the Skyrim installation)
  ├── loader::InstallPathRouting(gamePath)
  ├── steam::Load(gamePath)      (Steam integration)
  ├── LoadProgram(LC):
  │     ├── LoadFile(exePath)                    (reads SkyrimSE.exe from disk)
  │     ├── QueryFileVersion()                   (reads the EXE version)
  │     └── ExeLoader.Load(content) ─────────────► MAPS THE GAME INTO THE PROCESS
  │           └── GetEntryPoint() → LC.gameMain
  ├── InstallStartHook()         (client startup hook)
  ├── RunTiltedInit(gamePath, Version):
  │     ├── VersionDb::Load()    (Address Library — SKSE)
  │     ├── new TiltedOnlineApp()
  │     └── InstallHooks2() + TP_HOOK_COMMIT
  └── LC.gameMain()              ◄── enters the game; returns only when it exits
        │
        │  later during game startup, through a hook:
        ▼
      RunTiltedApp() → g_appInstance->BeginMain()   (client loop)
        │
        ▼
      OverlayService / OverlayClient  ──► CEF  ──► starts tp_process.exe (overlay)
```

The symbols that connect launcher and client are defined in the client and called by
the launcher. See
[`Launcher.cpp`](../Code/immersive_launcher/Launcher.cpp):

```cpp
extern void InstallStartHook();
extern void RunTiltedApp();
extern void RunTiltedInit(const std::filesystem::path&, const TiltedPhoques::String&);
```

## Where CEF is used

CEF (Chromium Embedded Framework) is the original in-game overlay/UI backend:

- `Code/tp_process/main.cpp` — CEF helper process; `WinMain` calls
  `TiltedPhoques::UIMain(...)` with a `ProcessHandler`. This is the CEF render/utility
  process.
- `Code/client/Services/OverlayService.*` and `OverlayClient.*` — client side that
  communicates with CEF and renders the overlay in-game.
- `Code/skyrim_ui/` — the TypeScript/Angular UI loaded inside CEF.

CEF was the main blocker for a decoupled launcher in the original portability
roadmap. The implemented Proton path now bypasses it in favor of ImGui; see
[cef-proton.md](cef-proton.md).

## Windows-specific inventory

### `immersive_launcher`

- `Main.cpp`: `Windows.h`, `combaseapi.h`, `LoadLibraryW`, `GetSystemDirectoryW`,
  `__declspec(dllexport)`, system DLL preloading, and COM (`ComScope`).
- `Launcher.cpp`: `ExeLoader` (manual PE mapping, intrinsically Windows/PE),
  `GetModuleHandleW`/`GetProcAddress`, `GetAsyncKeyState`, `SetLastError`, Win32
  `TaskDialog` through `Die()`, `steam::Load`, and the registry.
- `loader/` — `ExeLoader` and `PathRerouting`, which redirects game path calls.
- `usvfs/` — user-mode virtual file system through `usvfs_64.dll` injection.
- `script_extender/` — SKSE memory reservation.
- `ntdll_x64.lib` — direct link against ntdll.
- Win32 resources: `launcher.rc`, `launcher.aps`.

### `client`

- `main.cpp`: `Windows.h`, `Commctrl.h`, `TaskDialog`, `ShellExecuteW`, and `HICON`.
- `CrashHandler.*` — SEH/Win32-based crash handling.
- `imgui_impl_win32.cpp`, `imgui_impl_dx11.cpp` — Win32/DX11 rendering backends.
- Thousands of hooks targeting `SkyrimSE.exe` addresses through Address Library/SKSE.

### `tp_process`

- `main.cpp`: `WinMain` and `HINSTANCE`; Windows-only CEF process.
- `process.manifest`, `process.rc`.

## Central implication for the port

Because the client is code injected into the Windows game process (`SkyrimSE.exe`),
the realistic Linux strategy is not to rebuild it as a native ELF binary. The
launcher, game, and client run together under Proton/Wine, which provides the Windows
ABI.

The Linux work therefore focuses on:

1. the native server, which is already viable through upstream build paths;
2. making the launcher and injected client reliable inside a Proton prefix;
3. replacing or isolating Windows components that do not behave correctly under
   Wine, particularly the manual loader and CEF.

See [linux.md](linux.md) for the implemented architecture and current status.
