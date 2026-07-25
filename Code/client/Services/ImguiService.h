#pragma once

#include <imgui/ImGuiDriver.h>

struct RenderSystemD3D9;
struct RenderSystemD3D11;

/**
 * @brief Draws the ImGui UI.
 */
struct ImguiService
{
    using TCallback = void();

    ImguiService();
    ~ImguiService() noexcept;

    TP_NOCOPYMOVE(ImguiService);

    void Create(RenderSystemD3D11* apRenderSystem, HWND aHwnd);

    void Render() const;
    void Reset() const;

    LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RawInputHandler(RAWINPUT& aRawinput);
    void SetCursorControlEnabled(bool aEnabled) noexcept;
    void OnWindowFocusChanged(bool aFocused) noexcept;

    entt::sink<entt::sigh<TCallback>> OnDraw;

private:
    void ClampVirtualCursor() noexcept;
    void UpdateCursorClip() const noexcept;

    ImGuiImpl::ImGuiDriver m_imDriver;
    entt::sigh<TCallback> m_drawSignal;
    HWND m_window = nullptr;
    POINT m_virtualCursor{};
    RECT m_previousClipRect{};
    bool m_cursorControlEnabled = false;
    bool m_windowFocused = false;
    bool m_hasPreviousClipRect = false;
};
