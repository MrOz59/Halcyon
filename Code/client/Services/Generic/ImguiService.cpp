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
    m_imDriver.Initialize(static_cast<void*>(aHwnd));

    // init platform
    if (!ImGui_ImplWin32_Init(aHwnd))
        spdlog::error("Failed to initialize Imgui-Win32");

    ImGui_ImplDX11_Init(apRenderSystem->GetDevice(), apRenderSystem->GetDeviceContext());
}

void ImguiService::Render() const
{
    // Skyrim may continuously recenter or hide the OS cursor while it owns the
    // game camera. Keep the native overlay's virtual cursor authoritative before
    // the Win32 backend samples it.
    RestoreVirtualCursor();

    ImGui_ImplDX11_NewFrame();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_drawSignal.publish();
    ImGui::Render();

    if (m_cursorControlEnabled)
    {
        // The game can replace the cursor after ImGui's backend last changed its
        // shape. Refresh it every active frame instead of relying on a shape
        // transition that may never occur.
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
            RestoreVirtualCursor();
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

    m_cursorControlEnabled = aEnabled;
    if (!m_cursorControlEnabled || !m_window)
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
    RestoreVirtualCursor();
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

void ImguiService::RestoreVirtualCursor() const noexcept
{
    if (!m_cursorControlEnabled || !m_window)
        return;

    POINT target = m_virtualCursor;
    if (ClientToScreen(m_window, &target))
    {
        POINT current{};
        if (!GetCursorPos(&current) || current.x != target.x || current.y != target.y)
            SetCursorPos(target.x, target.y);
    }

    CURSORINFO cursorInfo{sizeof(cursorInfo)};
    if (!GetCursorInfo(&cursorInfo) || !(cursorInfo.flags & CURSOR_SHOWING))
    {
        while (ShowCursor(TRUE) < 0)
            ;
    }
}
