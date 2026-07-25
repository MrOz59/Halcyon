# Skyrim Together Reborn — Linux/Proton fork

[![Proton build](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml/badge.svg?branch=linux-port)](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml)
[![GPLv3 license](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

This fork adapts the **Skyrim Together Reborn** client to run Skyrim Special
Edition through Proton. Its goal is to preserve compatibility with official server
protocols while replacing only the paths that do not work reliably under Wine.

> This is an experimental community fork, not an official Skyrim Together release.
> The original project, its documentation, and its support channels are available at
> [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) and the
> [official wiki](https://wiki.tiltedphoques.com/tilted-online/).

## Current status

The main path has been validated in the development environment:

- Skyrim SE `1.6.1170` starts through the launcher inside Proton;
- new games and existing saves load;
- the interface opens and closes with `F2`;
- direct connections and the public server browser work;
- connection to vanilla servers using protocol `v1.8.0` has been confirmed;
- the player list and chat work, with chat in a separate window;
- text fields remain focused during normal overlay use and the cursor no longer
  competes with Skyrim's camera recentering;
- the Proton launcher can apply borderless, fullscreen, or windowed display mode.

The native interface also implements party management, invitations, chat channels,
and a persistent party HUD through the existing vanilla protocol. These expanded UI
flows should receive an in-game multiplayer pass in each new playable build.

### Validated launch methods

Runtime compatibility and the program used to start STR are separate concerns. The
following combinations have been validated:

| Launch method | Compatibility tool | Build | Result |
| --- | --- | --- | --- |
| Steam Non-Steam shortcut for `SkyrimTogether.exe` | Valve Proton 11.0 | `0954a161` | Game, UI, and vanilla server connection validated |
| Steam Non-Steam shortcut for `SkyrimTogether.exe` | Proton-CachyOS Latest | `0954a161` | Game, UI, and vanilla server connection validated |
| Steam Non-Steam shortcut for `SkyrimTogether.exe` | GE-Proton11-1 | `96dc3db2` | High-ASLR game image, UI, and vanilla server connection validated |
| Vortex tool launch with the runtime-refresh fix | Proton-CachyOS Latest | `96dc3db2` | Game, UI, and vanilla server connection validated |
| Vortex tool launch with the runtime-refresh fix | GE-Proton11-1 | `96dc3db2` | High-ASLR game image, UI, and vanilla server connection validated |

GE-Proton11-1 previously failed on `0954a161` because it loaded the game at a
randomized high address and Wine rewrote the mapped PE header's image base. The
relocation-aware validation change in `96dc3db` fixed that failure. These results are
specific to the versions tested; the names of custom compatibility tools and their
Wine bases can change between releases.

The client is **not a native Linux executable**. The launcher, game, and payload
remain Windows PE binaries and run inside the same Proton prefix. The dedicated
server already has a separate native Linux path in the upstream project.

## Installation

1. Open the
   [Linux-port playable build](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml)
   workflow and download the `SkyrimTogether-linux-port (linux-port-<commit>)`
   artifact from the latest successful run.
2. Install the artifact contents as a mod whose root maps to Skyrim's `Data`
   directory. This can be done through Vortex or manually. The resulting layout
   should include:

   ```text
   Skyrim Special Edition/
   └── Data/
       ├── SkyrimTogether.esp
       ├── SkyrimTogetherRebornBehaviors/
       ├── scripts/
       └── SkyrimTogetherReborn/
           └── SkyrimTogether.exe
   ```

3. Enable `SkyrimTogether.esp`.
4. In Steam, add
   `Data/SkyrimTogetherReborn/SkyrimTogether.exe` as a **Non-Steam Game**. Open that
   shortcut's **Properties > Compatibility**, force Valve Proton 11.0,
   GE-Proton11-1, or the tested Proton-CachyOS release, and launch STR from this
   shortcut.
5. Pass `--exePath` in the Steam shortcut's launch options only when the launcher
   cannot locate the game automatically, for example:

   ```text
   --exePath "Z:\path\to\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe"
   ```

Vortex can install, enable, deploy, and launch the mod when using the
runtime-selection refresh implemented in the Linux fork. Vortex launches using both
Proton-CachyOS and GE-Proton11-1 with Skyrim's `compatdata/489830/pfx` completed
vanilla server connections. If the compatibility tool was just changed in Steam,
wait for Steam to write `config/config.vdf` before starting the Vortex tool;
otherwise Vortex may still read the previous selection.

Runtime testing of build `4dfa345` confirmed stable text entry and smooth cursor
movement during normal native-UI use. The current tree additionally rebuilds raw-input
registration when focus returns after Alt+Tab.

On the first Proton launch, a small native display picker offers:

- **Borderless window** (recommended), using the current Proton desktop resolution;
- **Fullscreen**, using Skyrim's fullscreen mode at the desktop resolution;
- **Windowed**, preserving the resolution already stored by Skyrim;
- **Keep current Skyrim settings**, without editing the preferences file.

The picker changes only the `[Display]` keys in
`Documents/My Games/Skyrim Special Edition/SkyrimPrefs.ini`. Before its first
change, it preserves the original file as
`SkyrimPrefs.ini.skyrim-together.bak`. Select **Do not show this display picker
again** to reuse the chosen mode automatically. Run the launcher with `--configure`
to show it again.

After loading a save, press `F2` to open the interface. While connected, `Y`
opens the chat directly with its input field focused; `Enter` sends the message
and returns control to the game. `Y` is the default and can be changed under
**F2 > Settings > Controls**. `Esc` or `F2` also returns mouse and keyboard
control to the game. The internal ImGui debugger is available with `F3`; press
`F3` again or `Esc` to close it. The pointer is confined to the game client
while either ImGui layer owns input, but Alt+Tab releases it immediately.
Returning to the game re-registers raw mouse and keyboard input, so the open
interface does not need to be toggled to recover.

## Native interface

The CEF/Chromium runtime used by the original interface raises `0x80000003`
exceptions inside `libcef.dll` under Proton. GPU flags, software rendering, and an
alternative message pump were investigated, but none produced a stable path. Under
Wine, this fork does not initialize CEF and instead uses the ImGui/D3D11 renderer
that already existed in the client.

The native interface provides:

- hostname, IPv4, and IPv6 connections with password support;
- a public server browser with search, filters, favorites, and version warnings;
- connection status and more useful error messages;
- a player list with levels, locations, party status, invitations, and the
  `Reveal Players` action;
- global, party, and local chat in an independent window, including player
  dialogue, timestamps, message history, and the `/settime` command;
- party creation, invitation acceptance, membership management, leadership
  transfer, teleportation, and leaving;
- transient message and system notifications while the F2 menu is closed;
- a persistent party HUD with member health, location information, optional
  auto-hide, four anchors, and configurable offsets;
- optional compact network statistics with latency, packet loss, and traffic rates;
- a real ten-second cooldown for `Reveal Players`;
- configurable UI scale and HUD visibility;
- a visual theme inspired by Skyrim's menus;
- persistent favorites, filters, and native UI preferences in
  `Data/SkyrimTogetherReborn/native_overlay.json`.

On native Windows, the original CEF path remains available. CEF files may still be
present in the package for compatibility with that build, but they are not loaded
through the Wine/Proton path.

## What changed to make Proton work

### Game startup

The original manual loader mapped `SkyrimSE.exe` into the launcher process. Under
Wine, the unwind tables for this self-mapped image are not visible to
`RtlVirtualUnwind2`, causing crashes during otherwise normal game exceptions. The
launcher now detects Wine and uses:

```text
SkyrimTogether.exe
  -> CreateProcess(SUSPENDED)
  -> restores the Steam CEG-protected image
  -> injects STClientPayload.dll
  -> waits for client initialization
  -> resumes the SkyrimSE.exe main thread
```

The payload supports the external process, applies the required patches to the game
image, and uses relay thunks near target code for relative hooks that cannot reach the
payload directly.

Newer Proton versions can honor Skyrim's high-entropy ASLR flag and load the game far
from its preferred `0x140000000` image base. The launcher therefore validates the
remote PE headers obtained through the process PEB, applies supported base relocations
that target the decrypted `.text` bytes, and only then restores and verifies that
section. Relocations outside `.text` remain the responsibility of the Wine loader.
Wine may replace the in-memory PE header's preferred image base with the actual mapped
address; the validator accepts either exact value while keeping the signature,
architecture, image size, and protected entry point checks strict. Unknown image
layouts or relocation types fail before the game is resumed.

### Interface and input

Every CEF call is guarded when the runtime has not been initialized. The ImGui UI
reuses the client's D3D11 hook. While `F2` or the `F3` debugger is open, raw mouse
deltas update an ImGui-owned virtual cursor rendered in the same D3D11 frame; the
physical cursor is never repositioned every frame. Mouse, keyboard, character, and
raw-input messages consumed by ImGui are not dispatched a second time to Skyrim.
This prevents cursor lag, pointer snap-back, clicks leaking to another window, and
text fields losing focus because the game recaptured input.

The physical pointer is clipped to the game client only while the window is focused.
Alt+Tab releases that clip. On `WM_SETFOCUS`, the DirectInput hook explicitly
re-registers the raw mouse and keyboard devices because Wine may discard their
registration without changing the hook's logical state. Normal clicks do not trigger
this refresh, preserving the active ImGui text field.

### Networking

The network protocol was separated from the build identity. The fork advertises
`PROTOCOL_VERSION = v1.8.0`, while the commit remains visible in logs for diagnostics.
This prevents local changes from making the client artificially incompatible with
vanilla `v1.8.0` servers.

The `non_default_install` warning, which is common when Creations or additional
content are installed, is informational and does not block the connection attempt.
Mods must still be compatible between players and with the server's rules.

## Logs and diagnostics

The most useful files are:

- `logs/SkyrimTogether.log`, relative to the launcher's working directory
  (usually `Data/SkyrimTogetherReborn`) — Wine detection, startup strategy, CEG,
  and injection;
- `Skyrim Special Edition/logs/tp_client.log` — client initialization, protocol,
  authentication, loaded mods, and disconnections;
- `Data/SkyrimTogetherReborn/st_client_payload.log` — early payload stages;
- `Data/SkyrimTogetherReborn/st_beginmain_diag.log` — client entry markers.

The launcher also accepts `--verbose`, `--debug`, `--dump-config`, `--configure`,
`--skip-launcher-ui`, and
`--display-mode=borderless|fullscreen|windowed|current`. An explicit display mode
is saved and applied without opening the picker. For network failures, search
`tp_client.log` for the `connecting`, `Transport connected`, `authenticating`, and
`disconnected` lines.

A blank launcher console when STR is started from a Steam shortcut is not by itself
an error. File logging remains active; use the files above as the authoritative
startup record and add `--verbose` or `--debug` when more detail is required.

Common errors:

- `network_timeout`: the server is offline, its UDP port is blocked, or the route is
  unavailable;
- `cannot_resolve_address`: invalid hostname or a DNS failure inside the prefix;
- rejection during authentication: server version, password, or mod policy;
- the UI does not appear but input is captured: attach `tp_client.log` and confirm
  that the installed artifact matches the latest commit;
- the UI remains visible after Alt+Tab but the pointer does not move: search
  `tp_client.log` for
  `[input] raw input devices reacquired after window focus returned`. Its absence
  indicates an older artifact or a focus event that did not reach the game window.

## Known limitations

- The native UI covers the original gameplay and social actions plus the most
  important persistent HUD and notification settings. It does not reproduce the
  CEF interface's audio feedback, localization system, or every animation.
- Creations and gameplay mods can cause divergence even when the connection is
  accepted; use a compatible load order across all players.
- The public list may contain stale announcements. Appearing in the list does not
  guarantee that the server's UDP port is reachable.
- The path has been validated with Skyrim SE `1.6.1170`; other versions require
  testing.
- Vortex launch using Proton-CachyOS and GE-Proton11-1 has been validated with the
  runtime-selection refresh in the Linux Vortex fork.
- Proton versions, drivers, and mod managers differ across distributions, so the
  fork should still be considered experimental.

## Build and development

Every push to the `linux-port` branch, or a manual dispatch of
`linux-port-playable.yml`, builds the Windows binaries in release mode and produces
two artifacts:

- `SkyrimTogether-linux-port (...)` — playable package for the `Data` directory;
- `Debug Symbols (...)` — PDB symbols.

The historical `Code/linux_probe` loader/unwind diagnostic remains in the repository
for targeted investigations, but it is disabled by default, is not distributed by
the playable workflow, and is not required to run the mod. Developers can opt in
with `xmake config --linux_probe=y` and build the `LinuxProbeLoader` and
`LinuxProbePayload` targets explicitly.

Additional technical details are available in:

- [porting strategy and current status](docs/linux.md);
- [CEF decision record](docs/cef-proton.md);
- [launcher and client architecture](docs/architecture.md);
- [upstream build guide](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide).

Before contributing, read [CODE_GUIDELINES.md](CODE_GUIDELINES.md) and run
`clang-format` on modified C++ files.

## License

Tilted Online is free software licensed under the
[GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.html). See
[LICENSE](LICENSE).
