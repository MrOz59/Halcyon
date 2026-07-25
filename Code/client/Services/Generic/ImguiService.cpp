#include <TiltedOnlinePCH.h>

#include <Services/ImguiService.h>
#include <Systems/RenderSystemD3D11.h>
#include <d3d11.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui.h>

// According to imgui documentation we have to do it this way in order to avoid link conflicts with windows.h
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImguiService::ImguiService()
    : OnDraw(m_drawSignal)
{
}

ImguiService::~ImguiService() noexcept
{
}

void ImguiService::Create(RenderSystemD3D11* apRenderSystem, HWND aHwnd)
{
    m_window = aHwnd;
    m_windowFocused = GetForegroundWindow() == m_window;
    m_imDriver.Initialize(static_cast<void*>(aHwnd));

    // init platform
    if (!ImGui_ImplWin32_Init(aHwnd))
        spdlog::error("Failed to initialize Imgui-Win32");

    ImGui_ImplDX11_Init(apRenderSystem->GetDevice(), apRenderSystem->GetDeviceContext());
}

void ImguiService::Render() const
{
    if (m_cursorControlEnabled)
        UpdateCursorClip();

    ImGui_ImplDX11_NewFrame();

    ImGui_ImplWin32_NewFrame();

    // Skyrim and Wine may keep recentering the physical cursor while the game
    // camera owns the window. Feed ImGui the raw-input-backed virtual position
    // directly and draw its cursor in the overlay instead of fighting the game
    // with SetCursorPos every frame.
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = m_cursorControlEnabled;
    if (m_cursorControlEnabled)
        io.MousePos = ImVec2(static_cast<float>(m_virtualCursor.x), static_cast<float>(m_virtualCursor.y));

    ImGui::NewFrame();

    m_drawSignal.publish();
    ImGui::Render();

    if (m_cursorControlEnabled)
    {
        // Keep the physical cursor hidden if Skyrim changed it during the frame.
        // The visible pointer is part of ImGui's draw data.
        ImGui_ImplWin32_WndProcHandler(m_window, WM_SETCURSOR, 0, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImguiService::Reset() const
{
    // TODO: idk how imgui handles this
}

LRESULT ImguiService::WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}

void ImguiService::RawInputHandler(RAWINPUT& aRawinput)
{
    if (ImGui::GetCurrentContext() == NULL)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (aRawinput.header.dwType == RIM_TYPEMOUSE)
    {
        const auto mouse = aRawinput.data.mouse;

        if (m_cursorControlEnabled)
        {
            if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
            {
                const int screenWidth = std::max(1, GetSystemMetrics(mouse.usFlags & MOUSE_VIRTUAL_DESKTOP ? SM_CXVIRTUALSCREEN : SM_CXSCREEN));
                const int screenHeight = std::max(1, GetSystemMetrics(mouse.usFlags & MOUSE_VIRTUAL_DESKTOP ? SM_CYVIRTUALSCREEN : SM_CYSCREEN));
                POINT screenPosition{
                    static_cast<LONG>(static_cast<double>(mouse.lLastX) * screenWidth / 65535.0),
                    static_cast<LONG>(static_cast<double>(mouse.lLastY) * screenHeight / 65535.0),
                };

                if (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
                {
                    screenPosition.x += GetSystemMetrics(SM_XVIRTUALSCREEN);
                    screenPosition.y += GetSystemMetrics(SM_YVIRTUALSCREEN);
                }

                if (ScreenToClient(m_window, &screenPosition))
                    m_virtualCursor = screenPosition;
            }
            else
            {
                m_virtualCursor.x += mouse.lLastX;
                m_virtualCursor.y += mouse.lLastY;
            }

            ClampVirtualCursor();
        }

        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
        {
            io.MouseDown[0] = true;
        }

        if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
        {
            io.MouseDown[0] = false;
        }

        if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
        {
            io.MouseDown[1] = true;
        }

        if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
        {
            io.MouseDown[1] = false;
        }

        if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
        {
            io.MouseDown[2] = true;
        }

        if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
        {
            io.MouseDown[2] = false;
        }
    }
}

void ImguiService::SetCursorControlEnabled(bool aEnabled) noexcept
{
    if (m_cursorControlEnabled == aEnabled)
        return;

    if (aEnabled)
        m_hasPreviousClipRect = GetClipCursor(&m_previousClipRect) != FALSE;

    m_cursorControlEnabled = aEnabled;
    if (ImGui::GetCurrentContext() != NULL)
        ImGui::GetIO().MouseDrawCursor = m_cursorControlEnabled;

    if (!m_cursorControlEnabled)
    {
        if (m_windowFocused && m_hasPreviousClipRect)
            ClipCursor(&m_previousClipRect);
        else
            ClipCursor(nullptr);
        m_hasPreviousClipRect = false;
        return;
    }

    if (!m_window)
        return;

    POINT cursor{};
    if (GetCursorPos(&cursor) && ScreenToClient(m_window, &cursor))
        m_virtualCursor = cursor;
    else
    {
        RECT client{};
        GetClientRect(m_window, &client);
        m_virtualCursor = POINT{(client.right - client.left) / 2, (client.bottom - client.top) / 2};
    }

    ClampVirtualCursor();
    UpdateCursorClip();
}

void ImguiService::OnWindowFocusChanged(bool aFocused) noexcept
{
    m_windowFocused = aFocused;
    UpdateCursorClip();
}

void ImguiService::ClampVirtualCursor() noexcept
{
    if (!m_window)
        return;

    RECT client{};
    if (!GetClientRect(m_window, &client))
        return;

    m_virtualCursor.x = std::clamp<LONG>(m_virtualCursor.x, client.left, std::max(client.left, client.right - 1));
    m_virtualCursor.y = std::clamp<LONG>(m_virtualCursor.y, client.top, std::max(client.top, client.bottom - 1));
}

void ImguiService::UpdateCursorClip() const noexcept
{
    if (!m_cursorControlEnabled)
        return;

    if (!m_windowFocused || !m_window)
    {
        // Never keep the pointer trapped when the user switches away from the
        // game. The previous clip is restored when cursor control is disabled.
        ClipCursor(nullptr);
        return;
    }

    RECT client{};
    if (!GetClientRect(m_window, &client))
        return;

    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    if (!ClientToScreen(m_window, &topLeft) || !ClientToScreen(m_window, &bottomRight))
        return;

    RECT screenRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&screenRect);
}
