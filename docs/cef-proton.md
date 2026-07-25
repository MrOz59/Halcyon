# CEF under Proton/Wine — diagnosis and implemented decision

> Technical record of the blocker that led to the ImGui UI. The current conclusion
> is: **CEF is not initialized under Wine/Proton**. The native Windows path is
> preserved.

## Observed symptom

After the loader and payload were able to start the game, the process still exited
with exceptions such as:

```text
0x80000003 in libcef.dll
VectoredExceptionHandler: crash occurred
```

Addresses changed between runs but remained inside `libcef.dll`. In some attempts CEF
progressed further before the `int3`; in others, the exception occurred during
`CefInitialize` or browser creation.

## Initial hypothesis

The overlay uses `CefRenderHandler::OnPaint` and copies the CEF buffer to a D3D11
texture on the CPU. It therefore does not require `OnAcceleratedPaint`. The initial
hypothesis was that Chromium unnecessarily initialized ANGLE/GPU under Wine and
failed in the pipeline involving `D3DCompiler_47.dll`.

The following options were evaluated or implemented experimentally:

- `--disable-gpu` and `--disable-gpu-compositing`;
- software rendering/SwiftShader;
- disabling Chromium networking features;
- `single-process`;
- `external_message_pump`;
- explicit resolution of CEF resource directories;
- logs and markers around `CefInitialize` and `CreateBrowser`;
- isolated `libcef.dll` loading and diagnostic handlers.

These experiments were important for separating loader, payload, and game-loop
crashes. None made CEF reliable in the tested Proton environment.

## Decision

The project already had an ImGui renderer over D3D11 for internal tools. Reusing this
infrastructure is smaller and more predictable than maintaining an entire Chromium
runtime inside the Skyrim process under Wine.

When Wine is detected:

1. `OverlayService` does not create the CEF runtime;
2. events that would normally call CEF verify that it was initialized;
3. `ImGuiOverlayService` provides connections, public servers, players, chat,
   party management, invitations, and the persistent party HUD;
4. `InputService` routes keyboard, character, mouse, and raw-input messages to ImGui
   only while an ImGui layer is open and prevents Skyrim from processing them again;
5. `ImguiService` draws a raw-input-backed software cursor instead of moving the
   physical cursor against Skyrim's camera recentering;
6. cursor confinement is released on Alt+Tab and raw devices are re-registered when
   focus returns;
7. closing with `F2` or `Esc` returns input to the game.

On native Windows, the original CEF implementation is still built and used. CEF
dependencies and resources may therefore remain in the general artifact even though
the Proton path does not load them.

## Result

With CEF removed from the Wine path:

- new games and existing saves loaded without the `libcef.dll` `int3`;
- the overlay appeared in-game and released input correctly;
- text fields remained interactive without intermittent focus loss, and the cursor
  no longer lagged or snapped back during normal overlay use;
- the public list and authentication against a vanilla `v1.8.0` server worked;
- chat, the player list, and party actions used existing client services and
  vanilla protocol messages directly.

This result supports the decision for this fork, but does not prove that CEF cannot
run under every Wine version. It is simply no longer a functional dependency of the
Linux/Proton client.

## Native UI scope

The current ImGui UI covers the multiplayer and social actions used by the
TypeScript/CEF UI, including chat channels, party invitations and management,
teleportation, player reveal, time commands, notifications, chat history, compact
network statistics, and configurable party HUD behavior. CEF-specific audio,
localization, transitions, and animations are intentionally not part of the native
implementation.

The code in `Code/skyrim_ui`, `Code/tp_process`, and `Libraries/TiltedUI` remains
necessary for the native Windows build and as a behavioral reference.

The ImGui replacement deliberately owns only the input events needed by the active
overlay. System window shortcuts remain available, cursor confinement is removed
when the game loses focus, and a focused return forces Wine's raw-input registration
to be rebuilt. The detailed input lifecycle is documented in
[linux.md](linux.md#input-ownership-and-cursor-model).

## Future diagnostics

If a Proton crash points to CEF again, first confirm in the log that the build detected
Wine and reported that it skipped the CEF overlay. A `libcef.dll` load on this path
indicates a regression or an external DLL and should not be treated merely by adding
more Chromium flags.

See also:

- [port status](linux.md);
- [overall architecture](architecture.md);
- [installation README](../README.md).
