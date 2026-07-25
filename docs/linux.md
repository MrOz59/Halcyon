# Linux / Proton — port architecture and status

> Technical documentation for the path implemented on the `linux-port` branch.
> See the [README](../README.md) for installation and usage instructions.

## Goal

Make the Skyrim Together Reborn launcher and client work reliably under Wine/Proton
while preserving native Windows behavior and network compatibility with vanilla
`v1.8.0` servers.

The client remains Windows code injected into `SkyrimSE.exe`; converting it to ELF is
outside the scope of this port. Proton provides the required Win32, DX11, and
DirectInput ABI. The dedicated server has a separate native Linux path.

## Validated status

- [x] the launcher detects Wine and selects the external-process strategy;
- [x] Skyrim SE `1.6.1170` reaches the menu, starts a new game, and loads saves;
- [x] Steam CEG is restored before the original entry point;
- [x] the payload is injected and initialized before the game resumes;
- [x] out-of-range relative hooks use nearby relay thunks;
- [x] CEF is not initialized under Wine;
- [x] the ImGui overlay opens with `F2` and releases input correctly;
- [x] the internal ImGui debugger opens with `F3` and shares input capture safely;
- [x] ImGui text fields and its software cursor remain responsive without cursor
      recentering or input leaking to other windows;
- [x] Alt+Tab releases cursor confinement and focus return restores raw input;
- [x] the launcher applies a persistent Proton display mode, with borderless as the
      recommended default;
- [x] direct connections, the public list, favorites, players, and chat work;
- [x] connection to a vanilla `v1.8.0` server has been confirmed;
- [x] CI produces a playable package and debug symbols.

“Validated” describes the development environment used during this work. It is not a
guarantee for every combination of distribution, driver, Proton version, mod manager,
and load order.

## Validated Proton and launch methods

The compatibility tool configured for Skyrim in Steam is not necessarily the tool
used by another launcher. Runtime results must therefore record both the Proton
version and the launch path.

The following matrix records the completed runtime tests:

| Launch path | Actual runtime | Build | Result |
| --- | --- | --- | --- |
| Steam Non-Steam shortcut pointing to `SkyrimTogether.exe` | Valve Proton 11.0, reporting Wine 11.0 | `0954a161` | Game, UI, and vanilla `v1.8.0` server connection succeeded |
| Steam Non-Steam shortcut pointing to `SkyrimTogether.exe` | Proton-CachyOS Latest | `0954a161` | Game, UI, and vanilla `v1.8.0` server connection succeeded |
| Steam Non-Steam shortcut pointing to `SkyrimTogether.exe` | GE-Proton11-1 | `96dc3db2` | Randomized high-address image, game, UI, and vanilla `v1.8.0` server connection succeeded |
| Vortex tool entry with the runtime-refresh fix | Proton-CachyOS Latest, reporting Wine 11.0 | `96dc3db2` | Game, UI, and vanilla `v1.8.0` server connection succeeded |
| Vortex tool entry with the runtime-refresh fix | GE-Proton11-1 | `96dc3db2` | Randomized high-address image, game, UI, and vanilla `v1.8.0` server connection succeeded |

The remote-header change in commit `96dc3db` accepts Wine's relocated in-memory image
base while preserving the remaining PE validation checks. GE-Proton11-1 loaded Skyrim
at `0x6fffe7c10000`, compared with the preferred `0x140000000`, and completed the
entire CEG, payload, UI, and server-connection path. No `.text` relocation entry was
needed for Skyrim SE `1.6.1170`, matching the offline relocation-table analysis.

The current recommended path is:

1. use Vortex, another mod manager, or a manual installation to deploy the artifact;
2. add `Data/SkyrimTogetherReborn/SkyrimTogether.exe` to Steam as a Non-Steam game;
3. force Valve Proton 11.0, GE-Proton11-1, or the validated Proton-CachyOS release
   on that shortcut;
4. launch STR from Steam.

Build `4dfa345` was tested in-game with stable text fields and smooth software-cursor
movement during normal native-UI interaction. The subsequent focus-return change
adds forced raw-input re-registration for the Alt+Tab case without restoring
per-click hook resets.

## Why the original launcher failed

The upstream path uses `ExeLoader` to map the game image inside the launcher process.
Under Wine, the unwind tables for this self-mapped image are not visible to
`RtlVirtualUnwind2`. Exceptions that Windows would normally unwind caused a crash
during early game startup.

Under Wine, `GameLauncherFactory` selects `ExternalProcessLauncher`:

1. create `SkyrimSE.exe` as a real suspended process;
2. prepare the Steam CEG-protected image and restore its decrypted region;
3. inject `STClientPayload.dll`;
4. let the payload locate and apply hooks, then signal completion;
5. resume the main thread at the original entry point.

Because the payload may be more than ±2 GiB away from game code, `rel32` hooks use
relay thunks allocated near their target addresses.

### Proton 11 and high-entropy ASLR

Skyrim SE declares both `DYNAMIC_BASE` and `HIGH_ENTROPY_VA`. Older Proton
configurations observed during development placed the executable at its preferred
`0x140000000` base, while Proton 11 may place it at a randomized high address.

The external launcher reads the actual image base from the suspended process PEB and
validates the remote DOS and PE headers against the selected executable. If the base
differs, it parses the executable's base-relocation directory and applies supported
`DIR64` fixups only when they target the decrypted `.text` range. The Wine loader has
already relocated all other sections. The launcher then writes the decrypted code,
flushes the instruction cache, restores page protection, and verifies the remote
bytes before patching the protected entry point.

Wine can rewrite the mapped PE header's `OptionalHeader.ImageBase` from the preferred
file value to the actual randomized address before applying relocations. Remote image
validation therefore accepts either the preferred base or the exact PEB-resolved
mapped base. It still requires matching PE signatures, AMD64 architecture, image
size, and protected entry-point RVA, and logs every compared value on failure.

For Skyrim SE `1.6.1170`, the relocation table contains no entries inside the main
`.text` section, so a Proton 11 high-address load requires validation but no code
fixups. Keeping the parser generic avoids reintroducing the fixed-base assumption for
other supported executable layouts. Malformed images and unknown relocation types
inside `.text` are rejected before the game resumes.

In the current tests, Valve Proton 11.0 mapped Skyrim at its preferred base and
completed launch and server connection. GE-Proton11-1 randomized the image base,
exposing the remote-header validation issue in `0954a161`; the same scenario completed
successfully with `96dc3db2`. “Proton 11” is therefore not a single loader behavior,
even though both tested distributions are now supported.

## Why CEF was removed from the Proton path

The embedded CEF runtime raises `0x80000003` inside `libcef.dll` under Proton. The
investigation covered flags that disabled GPU and networking, SwiftShader,
single-process mode, an external message pump, and detailed instrumentation. These
experiments changed where the crash occurred, but did not produce a stable startup.

The final decision is platform-specific:

- **Wine/Proton:** do not initialize CEF; use the ImGui UI through the existing D3D11
  hook;
- **native Windows:** preserve the upstream CEF overlay.

CEF overlay calls are guarded when its runtime is unavailable. This prevents later
input, render, or game events from accessing partially initialized objects. The
detailed history is recorded in [cef-proton.md](cef-proton.md).

## Proton display configuration

The launcher leaves native Windows behavior unchanged. Under Wine/Proton,
`DisplaySettings` presents a small native picker on first launch and stores the
selection in the existing per-user launcher registry key. The available modes are
borderless, fullscreen, windowed, and unchanged.

For borderless and fullscreen, the launcher uses the current Proton desktop
resolution. It updates only `bBorderless`, `bFull Screen`, `iSize W`, and `iSize H`
under the `[Display]` section of:

```text
Documents/My Games/Skyrim Special Edition/SkyrimPrefs.ini
```

An existing preferences file is copied once to
`SkyrimPrefs.ini.skyrim-together.bak` before modification. The picker can be hidden
after a selection and reopened with `--configure`.

## Vortex launch path

The tested native Vortex AppImage applies its own Skyrim Together launch workaround.
An earlier build cached a compatibility-tool path and continued to invoke:

```text
.../Steam/compatibilitytools.d/GE-Proton11-1/files/bin/wineserver
```

and using Skyrim's prefix:

```text
.../Steam/steamapps/compatdata/489830/pfx
```

after Skyrim was changed to Valve Proton 11 and Proton-CachyOS in Steam. Those
failures were therefore additional GE-Proton11-1 attempts, not regressions reproduced
under Valve or CachyOS Proton.

The Vortex Linux fork now refreshes Steam's compatibility mapping immediately before
launch. A Proton-CachyOS launch through this path reached a vanilla server. During a
subsequent test, however, the Proton process started at `17:39:01` and Steam wrote the
new `489830 -> GE-Proton11-1` mapping at `17:39:03.848`; Vortex correctly launched
the previous CachyOS selection because the new value was not yet on disk. Wait for
Steam to finish writing `config/config.vdf` after changing Proton.

Vortex remains suitable for installing, deploying, and launching the mod. The
corrected path was validated with both Proton-CachyOS and GE-Proton11-1. In the GE
test, the process environment, Proton command, and `wineserver` all resolved to
GE-Proton11-1; the game then completed high-ASLR CEG preparation, opened the UI, and
authenticated with a vanilla `v1.8.0` server.

## ImGui UI

The native service receives the same events and uses the same client transport
services:

- direct connections by hostname, IPv4/IPv6, and password;
- public browser through `skyrim-reborn-list.skyrim-together.com/list`;
- search, filters, favorites, and visual version validation;
- player names, levels, locations, health, and party state;
- global, party, and local chat in a separate window, with timestamps, wrapped
  messages, input history, and an `Enter` shortcut that focuses chat directly;
- player dialogue plus `/help`, `/global`, `/local`, `/party`, and `/settime`;
- the `Reveal Players` action;
- party creation, incoming and outgoing invitations, membership management,
  leadership transfer, teleportation, and leaving;
- transient chat/system notifications and an invite notice while the interactive
  menu is closed;
- a persistent party HUD with optional auto-hide, 1/3/5 second delays, four
  anchors, and edge offsets;
- optional compact latency, packet-loss, and bandwidth statistics;
- an enforced ten-second `Reveal Players` cooldown;
- configurable UI scale, HUD visibility, and access to the `F3` debugger;
- DirectInput capture and release synchronized with visibility and window focus;
- settings stored in `Data/SkyrimTogetherReborn/native_overlay.json`.

The native implementation uses the existing client events, party service, and wire
messages. It does not add fork-specific network messages, so these actions remain
compatible with vanilla `v1.8.0` servers.

### Input ownership and cursor model

Skyrim, Wine, and the Win32 ImGui backend share one game window, but only one of them
may own interactive input at a time. The Proton path uses this sequence while the
`F2` overlay or `F3` debugger is visible:

1. `DInputHook` suppresses the keyboard and mouse data returned to Skyrim;
2. `InputService` forwards Win32 and raw-input events to ImGui;
3. input messages handled by ImGui are consumed before Skyrim's original window
   procedure receives them;
4. relative raw mouse deltas update a client-space virtual cursor;
5. `ImguiService` writes that position to `ImGuiIO::MousePos` and draws the pointer
   through `MouseDrawCursor` in the D3D11 frame.

The implementation does not call `SetCursorPos` every frame. That earlier approach
fought Skyrim's camera recentering under Wine, which caused cursor lag, snap-back,
intermittent text-field deactivation, and occasional clicks outside the game. The
physical pointer is now hidden and clipped to the game client while the window is
focused, preventing an invisible host cursor from clicking the launcher console or
desktop.

`WM_KILLFOCUS` releases cursor confinement so Alt+Tab remains usable. When
`WM_SETFOCUS` returns with an ImGui layer still open, `InputService` forces
`DInputHook::Update()` even though its logical enabled state did not change. This
re-registers raw mouse and keyboard devices that Wine may have silently dropped.
Ordinary mouse clicks use the non-forced refresh path and therefore do not disturb
the active ImGui text field.

## Network compatibility

The Git commit identifies the build in logs and executable resources, but must not be
used as the wire-protocol version. This fork defines:

```cpp
#define PROTOCOL_VERSION "v1.8.0"
```

Both client and server use this value during authentication and public-list
announcement. This allows the fork to connect to vanilla `v1.8.0` servers without
hiding the local commit in use.

The `non_default_install` warning is informational. Creations and additional mods can
still be rejected by server policy or cause gameplay divergence.

## Instrumentation

The launcher accepts:

- `--verbose` — use the `debug` log level;
- `--debug` — use the `trace` log level and show the console;
- `--dump-config` — print the resolved configuration without starting the game;
- `--configure` — reopen the Proton display picker;
- `--skip-launcher-ui` — apply the saved mode without showing the picker;
- `--display-mode=borderless|fullscreen|windowed|current` — save and apply a mode
  without showing the picker.

In-game UI shortcuts:

- `F2` — toggle the native multiplayer overlay;
- `Enter` — open and focus chat while connected;
- `F3` — toggle the internal ImGui debugger;
- `Esc` — close the active debug layer or overlay.

Primary files:

- `logs/SkyrimTogether.log`, relative to the launcher's working directory;
- `Skyrim Special Edition/logs/tp_client.log`;
- `Data/SkyrimTogetherReborn/st_client_payload.log`;
- `Data/SkyrimTogetherReborn/st_beginmain_diag.log`.

The client records the endpoint, route type (direct IP or name resolution), protocol,
commit, loaded-mod count, and authentication stage. Timeouts, DNS failures, and local
network problems are translated into distinct UI messages.

After returning from Alt+Tab while the native UI is open, a successful input recovery
adds this client-log line:

```text
[input] raw input devices reacquired after window focus returned
```

If the UI remains visible but its mouse does not move, verify that this line appears
after the focus return and that the installed artifact contains the expected commit.

The launcher console may remain blank when Steam owns the Proton process. This is not
a failure signal when the game and UI continue to load; the file logs above remain
authoritative. Use `--verbose` or `--debug` to request additional launcher detail.

## Build and distribution

The [linux-port-playable.yml](../.github/workflows/linux-port-playable.yml) workflow
runs a Windows release build because that is the format consumed by Proton. The
`str-build` package already has the layout expected inside the `Data` directory.

The upstream native-server build has two paths:

- Docker: [Dockerfile](../Dockerfile) and [MakeLinux.cmd](../MakeLinux.cmd);
- xmake/Nix: [flake.nix](../flake.nix), followed by
  `xmake f -p linux -a x64` and `xmake build SkyrimTogetherServer`.

Do not confuse the Windows-for-Proton job with the dedicated server's native Linux
build.

### Optional loader diagnostic

`Code/linux_probe` is a historical loader/unwind diagnostic retained for targeted
Proton investigations. It is not part of the runtime, is excluded from normal builds
and release artifacts, and is not required to launch or use the mod.

To make its two Windows PE targets available explicitly:

```text
xmake config --arch=x64 --mode=release --linux_probe=y
xmake build LinuxProbeLoader LinuxProbePayload
```

See [Code/linux_probe/README.md](../Code/linux_probe/README.md) for its purpose and
limitations.

## Next steps

- complete runtime validation across other Proton 11 builds and different drivers;
- validate the complete party workflow across several vanilla servers;
- add optional UI audio/localization parity without introducing CEF;
- automate a payload smoke test in addition to build/link validation;
- improve integration with Lutris, Bottles, and Steam Deck;
- review whether CEF artifacts are needed in a Proton-only package.
