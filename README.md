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
- the player list and chat work, with chat in a separate window.
- the Proton launcher can apply borderless, fullscreen, or windowed display mode.

The native interface also implements party management, invitations, chat channels,
and a persistent party HUD through the existing vanilla protocol. These expanded UI
flows should receive an in-game multiplayer pass in each new playable build.

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

3. Enable `SkyrimTogether.esp` and use the same Proton prefix and version configured
   for Skyrim.
4. Register `Data/SkyrimTogetherReborn/SkyrimTogether.exe` as a tool in your mod
   manager. Pass `--exePath` when the launcher cannot locate the game automatically,
   for example:

   ```text
   --exePath "Z:\path\to\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe"
   ```

5. Start that tool from the environment that already applies Skyrim's Proton prefix.
   Do not run the launcher in a different Wine prefix.

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

After loading a save, press `F2` to open the interface. While connected, `Enter`
opens the chat directly with its input field focused; sending the message returns
control to the game. `Esc` or `F2` also returns mouse and keyboard control to the
game. The internal ImGui debugger is available with `F3`; press `F3` again or `Esc`
to close it.

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

### Interface and input

Every CEF call is guarded when the runtime has not been initialized. The ImGui UI
reuses the client's D3D11 hook, while DirectInput routing was adjusted so `F2` and
the `F3` debugger share input capture safely and always return control to the game
when both interfaces are closed.

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

Common errors:

- `network_timeout`: the server is offline, its UDP port is blocked, or the route is
  unavailable;
- `cannot_resolve_address`: invalid hostname or a DNS failure inside the prefix;
- rejection during authentication: server version, password, or mod policy;
- the UI does not appear but input is captured: attach `tp_client.log` and confirm
  that the installed artifact matches the latest commit.

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
- Proton versions, drivers, and mod managers differ across distributions, so the
  fork should still be considered experimental.

## Build and development

Every push to the `linux-port` branch, or a manual dispatch of
`linux-port-playable.yml`, builds the Windows binaries in release mode and produces
three artifacts:

- `SkyrimTogether-linux-port (...)` — playable package for the `Data` directory;
- `Debug Symbols (...)` — PDB symbols;
- `linux-probe (...)` — historical loader diagnostic tool.

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
