# Skyrim Together UI

This Angular application is the original CEF-backed Skyrim Together interface. It
remains part of the native Windows build and is also the behavioral reference for
the Proton replacement.

The Linux/Proton client does not initialize CEF. Its in-game interface is implemented
by `Code/client/Services/Generic/ImGuiOverlayService.cpp` and uses the existing D3D11
ImGui renderer. Changes intended for both platforms must account for both UI paths;
do not assume that modifying this Angular application changes the Proton interface.

See:

- [the repository Proton guide](../../docs/linux.md#imgui-ui);
- [the CEF decision record](../../docs/cef-proton.md);
- [the official UI build guide](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide#building-the-together-ui).
