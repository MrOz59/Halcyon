# linux_probe — validating the Proton unwind hypothesis

This is a **diagnostic tool**, not part of the product. It was created to answer one
question before the external-process refactor:

> Did Skyrim crash under Proton because `SkyrimSE.exe` was **self-mapped** by
> `ExeLoader` through manual PE mapping, leaving its unwind tables invisible to
> Wine's `RtlVirtualUnwind2`?

## Hypothesis

The investigation at the time, recorded in `docs/cef-proton.md` and the history of
the `linux-port` branch, identified this sequence:

1. Inside `gameMain()`, Skyrim names threads through the legacy MSVC mechanism:
   `RaiseException(0x406D1388)` = `EXCEPTION_WINE_NAME_THREAD`.
2. Windows absorbs this when a debugger is attached. Under Wine without a debugger,
   the exception is actually dispatched.
3. A vectored handler inside the game returns `EXCEPTION_CONTINUE_SEARCH`, so
   `RtlVirtualUnwind2` walks the stack and enters self-mapped code at base
   `0x140000000`.
4. It cannot find usable unwind information there, follows invalid RIP values (`0`,
   `0x1000`, `0x19c`), and the cascade ends in SIGFPE with exit code 136.

The root cause was not exception `0x406D1388` itself. The exception/unwind table for a
self-mapped module was not registered in the form required by Wine. `ExeLoader.cpp`
acknowledged this limitation:

```cpp
// has no use - inverted function tables get used instead from Ldr;
// we have no influence on those
RtlAddFunctionTable(...);
```

`RtlAddFunctionTable` registers a dynamic table, which is the second lookup choice.
The first is the loaded module table (`LdrpInvertedFunctionTable`), which is never
populated for a memory region that the loader does not recognize as an image.

Consuming `0x406D1388` in a vectored handler would remove only one trigger. Skyrim,
the CRT, and D3D raise legitimate SEH exceptions elsewhere, and each would encounter
the same broken unwind path. The probe therefore tested the structural hypothesis
rather than that workaround.

## What the probe does

If the hypothesis was correct, allowing the **Wine loader** to load `SkyrimSE.exe`
through a normal `CreateProcess` call would solve the problem because Wine would
populate its own image structures. The probe creates exactly that minimal scenario:

- `probe_loader.exe` — creates `SkyrimSE.exe` **suspended**, injects
  `probe_payload.dll` through `CreateRemoteThread` + `LoadLibraryW`, then calls
  `ResumeThread`. This is the same early execution window used by SKSE: the DLL runs
  before the game's entry point.
- `probe_payload.dll` — does **not** load the STR client or install hooks. It only
  installs a vectored exception handler that logs every dispatched exception,
  including its code, address, and module, then returns
  `EXCEPTION_CONTINUE_SEARCH` without changing behavior.

The goal was to isolate the **loading mode** variable. No STR client code runs inside
the game during this test.

## Interpreting the result

| Observation | Interpretation |
| --- | --- |
| The game reaches the main menu; the log shows `0x406D1388` being dispatched and continued without a crash | **Hypothesis confirmed.** Self-mapping is the cause and the external-injection refactor is justified. |
| The game crashes in the same way (SIGFPE / exit 136) while unwind follows invalid RIP values | **Hypothesis rejected.** Loading mode is not the cause, so the full refactor can be avoided. |
| The game crashes differently, for example because Steam DRM/CEG rejects the process | Partial result: unwind may be fixed, but another blocker remains. The handler log identifies it. |

The third outcome was the known caveat. STR already handled Steam CEG DRM through
`steam/SteamCeg.cpp` and `steam/SteamCrypto.h`, but the anti-tamper behavior with the
EXE running as a real process instead of being manually mapped had not yet been
established. The probe answered that question at low cost.

## Running the probe under Proton

The binaries are Windows PE files and must come from CI because this development
machine does not have a local PE build toolchain. Download the artifact and place both
files together in any directory accessible from the prefix.

```bash
export PROTON="$HOME/.local/share/Steam/compatibilitytools.d/GE-Proton11-1/proton"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
export STEAM_COMPAT_DATA_PATH="$HOME/.local/share/Steam/steamapps/compatdata/489830"
export STEAM_COMPAT_APP_ID=489830
export PROTON_LOG=1
export WINEDEBUG=+seh,+unwind

"$PROTON" run /path/to/probe_loader.exe "C:\\path\\to\\SkyrimSE.exe"
```

Collect these outputs:

- `probe_loader.log` and `probe_payload.log`, created next to `probe_loader.exe`;
- `~/steam-489830.log`, created by `PROTON_LOG=1`, which contains the
  `+seh,+unwind` trace.

`WINEDEBUG=+seh,+unwind` makes the result readable. It shows whether
`RtlVirtualUnwind2` still follows invalid RIP values after entering the game image.

## Deliberate limitations

This probe is **not** a production path and should not become one. It does not provide
usvfs/PathRerouting, reserve the Script Extender memory range, install API stubs, or
load the client. A production `ExternalProcessLauncher` needs all of those pieces.

The hypothesis was ultimately confirmed, and the production implementation now lives
in `Code/immersive_launcher/launch/ExternalProcessLauncher.*` and
`Code/client_payload/`.
