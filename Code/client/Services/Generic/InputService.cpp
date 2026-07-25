#include <TiltedOnlinePCH.h>

#include <Services/InputService.h>
#include <Services/ImGuiOverlayService.h>
#include <Services/OverlayService.h>

#include <OverlayApp.hpp>

#include <DInputHook.hpp>
#include <WindowsHook.hpp>

#include <include/internal/cef_types.h>
#include <Services/DebugService.h>
#include <Services/ImguiService.h>
#include <Services/DiscordService.h>
#include <World.h>

#include "Games/Skyrim/Interface/MenuControls.h"

static OverlayService* s_pOverlay = nullptr;
static UINT s_currentACP = CP_ACP;
static bool s_suppressChatOpenCharacter = false;

void ForceKillAllInput()
{
    MenuControls::GetInstance()->SetToggle(false);
}

uint32_t GetCefModifiers(uint16_t aVirtualKey)
{
    uint32_t modifiers = EVENTFLAG_NONE;

    if (GetKeyState(VK_MENU) & 0x8000)
    {
        modifiers |= EVENTFLAG_ALT_DOWN;
    }

    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        modifiers |= EVENTFLAG_CONTROL_DOWN;
    }

    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        modifiers |= EVENTFLAG_SHIFT_DOWN;
    }

    if (GetKeyState(VK_LBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_RBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_MBUTTON) & 0x8000)
    {
        modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    }

    if (GetKeyState(VK_CAPITAL) & 1)
    {
        modifiers |= EVENTFLAG_CAPS_LOCK_ON;
    }

    if (GetKeyState(VK_NUMLOCK) & 1)
    {
        modifiers |= EVENTFLAG_NUM_LOCK_ON;
    }

    if (aVirtualKey)
    {
        if (aVirtualKey == VK_RCONTROL || aVirtualKey == VK_RMENU || aVirtualKey == VK_RSHIFT)
        {
            modifiers |= EVENTFLAG_IS_RIGHT;
        }
        else if (aVirtualKey == VK_LCONTROL || aVirtualKey == VK_LMENU || aVirtualKey == VK_LSHIFT)
        {
            modifiers |= EVENTFLAG_IS_LEFT;
        }
        else if (aVirtualKey >= VK_NUMPAD0 && aVirtualKey <= VK_DIVIDE)
        {
            modifiers |= EVENTFLAG_IS_KEY_PAD;
        }
    }

    return modifiers;
}

// Remember to update DInputHook::SetToggleKeys when changing these shortcuts.
bool IsOverlayToggleKey(int aKey) noexcept
{
    return aKey == VK_RCONTROL || aKey == VK_F2;
}

bool IsDebugToggleKey(int aKey) noexcept
{
    return aKey == VK_F3;
}

bool IsDisableKey(int aKey) noexcept
{
    return aKey == VK_ESCAPE;
}

bool IsSystemWindowShortcut(UINT aMessage, WPARAM aKey) noexcept
{
    if (aMessage != WM_SYSKEYDOWN && aMessage != WM_SYSKEYUP)
        return false;

    return aKey == VK_TAB || aKey == VK_RETURN || aKey == VK_F4;
}

bool IsImGuiInputMessage(UINT aMessage, WPARAM aKey) noexcept
{
    if (IsSystemWindowShortcut(aMessage, aKey))
        return false;

    switch (aMessage)
    {
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SETCURSOR: return true;
    default: return false;
    }
}

void SetSystemCursorVisible(bool aVisible) noexcept
{
    CURSORINFO cursorInfo{sizeof(cursorInfo)};
    if (GetCursorInfo(&cursorInfo))
    {
        const bool isVisible = (cursorInfo.flags & CURSOR_SHOWING) != 0;
        if (isVisible == aVisible)
            return;
    }

    if (aVisible)
    {
        while (ShowCursor(TRUE) < 0)
            ;
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
            ;
    }
}

void InputService::RefreshInputState(bool aReacquireRawInput) noexcept
{
    if (!s_pOverlay)
        return;

    auto& world = World::Get();
    const bool debugVisible = world.GetDebugService().IsVisible();
    const auto* pNativeOverlay = world.GetImGuiOverlayService();
    const bool regularUiVisible = pNativeOverlay ? pNativeOverlay->IsVisible() : s_pOverlay->GetActive();
    const bool inputActive = regularUiVisible || debugVisible;

    // Re-registering the raw input devices during ordinary overlay interaction
    // can make ImGui lose the active text field. Normally only touch the hook
    // when ownership changes; after a real window-focus return, force a refresh
    // because Wine may have silently discarded the registration.
    auto& inputHook = TiltedPhoques::DInputHook::Get();
    if (inputHook.IsEnabled() != inputActive)
        inputHook.SetEnabled(inputActive);
    else if (aReacquireRawInput && inputActive)
    {
        inputHook.Update();
        spdlog::info("[input] raw input devices reacquired after window focus returned");
    }

    // The Proton path and the standalone F3 debugger use a software cursor drawn
    // by ImGui. Keeping the physical cursor hidden prevents Skyrim and Wine from
    // fighting over its position while the overlay owns input.
    const bool drawImGuiCursor = inputActive && (pNativeOverlay || !s_pOverlay->GetActive());
    world.ctx().at<ImguiService>().SetCursorControlEnabled(drawImGuiCursor);
    SetSystemCursorVisible(false);
}

void SetUIActive(OverlayService& aOverlay, auto apRenderer, bool aActive)
{
    aOverlay.SetActive(aActive);

    // Ensures the game is actually loaded, in case the initial event was sent too early
    aOverlay.SetVersion(BUILD_COMMIT);
    if (auto* pApp = aOverlay.GetOverlayApp()) // Null under Wine because CEF is skipped.
        pApp->ExecuteAsync("enterGame");

    apRenderer->SetCursorVisible(aActive);
    InputService::RefreshInputState();
}

void ProcessKeyboard(uint16_t aKey, uint16_t aScanCode, cef_key_event_type_t aType, bool aE0, bool aE1)
{
    if (aType != KEYEVENT_CHAR)
    {
        if (!aKey || aKey == 255)
        {
            return;
        }

        if (aKey == VK_SHIFT)
        {
            aKey = static_cast<uint16_t>(MapVirtualKey(aScanCode, MAPVK_VSC_TO_VK_EX));
        }
        else if (aKey == VK_NUMLOCK)
        {
            aScanCode = static_cast<uint16_t>(MapVirtualKey(aKey, MAPVK_VK_TO_VSC) | 0x100);
        }

        if (aE1)
        {
            if (aKey == VK_PAUSE)
            {
                aScanCode = 0x45;
            }
            else
            {
                aScanCode = static_cast<uint16_t>(MapVirtualKey(aKey, MAPVK_VK_TO_VSC));
            }
        }

        if (aE0)
        {
            switch (aKey)
            {
            case VK_CONTROL: aKey = VK_RCONTROL; break;
            case VK_MENU: aKey = VK_RMENU; break;
            case VK_RETURN: aKey = VK_SEPARATOR; break;
            }
        }
        else
        {
            switch (aKey)
            {
            case VK_CONTROL: aKey = VK_LCONTROL; break;
            case VK_MENU: aKey = VK_LMENU; break;
            case VK_INSERT: aKey = VK_NUMPAD0; break;
            case VK_DELETE: aKey = VK_DECIMAL; break;
            case VK_HOME: aKey = VK_NUMPAD7; break;
            case VK_END: aKey = VK_NUMPAD1; break;
            case VK_PRIOR: aKey = VK_NUMPAD9; break;
            case VK_NEXT: aKey = VK_NUMPAD3; break;
            case VK_LEFT: aKey = VK_NUMPAD4; break;
            case VK_RIGHT: aKey = VK_NUMPAD6; break;
            case VK_UP: aKey = VK_NUMPAD8; break;
            case VK_DOWN: aKey = VK_NUMPAD2; break;
            case VK_CLEAR: aKey = VK_NUMPAD5; break;
            }
        }
    }

    auto& overlay = *s_pOverlay;
    auto& debugUi = World::Get().GetDebugService();

    if (aType != KEYEVENT_CHAR && IsDebugToggleKey(aKey))
    {
        if (!overlay.GetInGame())
            debugUi.SetVisible(false);
        else if (aType == KEYEVENT_KEYUP)
            debugUi.Toggle();

        return;
    }

    if (aType != KEYEVENT_CHAR && IsDisableKey(aKey) && debugUi.IsVisible())
    {
        if (aType == KEYEVENT_KEYUP)
            debugUi.SetVisible(false);

        return;
    }

    // Wine/Proton has no CEF overlay, so F2 toggles the native ImGui UI. DInput
    // captures the keyboard on keydown; always reconcile state when the key
    // reaches this path so keyboard and mouse cannot remain captured.
    if (auto* pImGuiOverlay = World::Get().GetImGuiOverlayService())
    {
        const bool uiVisible = pImGuiOverlay->IsVisible();
        const bool isToggle = IsOverlayToggleKey(aKey);
        const bool isClose = IsDisableKey(aKey) && uiVisible;

        if (aType != KEYEVENT_CHAR && (isToggle || isClose))
        {
            if (!overlay.GetInGame())
            {
                pImGuiOverlay->SetVisible(false);
            }
            else if (aType == KEYEVENT_KEYUP)
            {
                if (isToggle)
                    pImGuiOverlay->Toggle();
                else
                    pImGuiOverlay->SetVisible(false);
            }
        }

        return; // No CEF browser exists on this path.
    }

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    spdlog::debug("ProcessKey, type: {}, key: {}, active: {}", aType, aKey, active);

    if (aType != KEYEVENT_CHAR && (IsOverlayToggleKey(aKey) || (IsDisableKey(aKey) && active)))
    {
        if (!overlay.GetInGame())
        {
            TiltedPhoques::DInputHook::Get().SetEnabled(false);
        }
        else if (aType == KEYEVENT_KEYUP)
        {
            SetUIActive(overlay, pRenderer, !active);
        }
    }
    else if (active)
    {
        pApp->InjectKey(aType, GetCefModifiers(aKey), aKey, aScanCode);
    }
}

void ProcessMouseMove(uint16_t aX, uint16_t aY)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (active)
    {
        pApp->InjectMouseMove(aX, aY, GetCefModifiers(0));
    }
}

void ProcessMouseButton(uint16_t aX, uint16_t aY, cef_mouse_button_type_t aButton, bool aDown)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (active)
    {
        pApp->InjectMouseButton(aX, aY, aButton, !aDown, GetCefModifiers(0));
    }
}

void ProcessMouseWheel(uint16_t aX, uint16_t aY, int16_t aZ)
{
    auto& overlay = *s_pOverlay;

    const auto pApp = overlay.GetOverlayApp();
    if (!pApp)
        return;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    const auto active = overlay.GetActive();

    if (active)
    {
        pApp->InjectMouseWheel(aX, aY, aZ, GetCefModifiers(0));
    }
}

UINT GetRealACP()
{
    // Get the keyboard layout for the current thread.
    HKL keybdLayout = GetKeyboardLayout(0);

    // Extract the language ID from it, contained in its low-order word.
    int langID = LOWORD(keybdLayout);

    // Call the GetLocaleInfo function to retrieve the default ANSI code page
    // associated with that language ID.
    UINT acp = CP_ACP;
    GetLocaleInfo(MAKELCID(langID, SORT_DEFAULT),
        LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
        (LPTSTR) &acp,
        sizeof(acp) / sizeof(TCHAR));
    return acp;
}

LRESULT CALLBACK InputService::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Under Wine/Proton there is no CEF overlay. Forward window events to ImGui
    // whenever either the F2 overlay or the F3 debugger is visible. WM_INPUT must
    // remain active because it delivers the toggle keyup after DInput captures the
    // keyboard.
    if (auto* pImGuiOverlay = World::Get().GetImGuiOverlayService())
    {
        const bool imguiInputActive = pImGuiOverlay->IsVisible() || World::Get().GetDebugService().IsVisible();

        if (s_suppressChatOpenCharacter)
        {
            if (uMsg == WM_CHAR && wParam == VK_RETURN)
            {
                s_suppressChatOpenCharacter = false;
                return 1;
            }

            if (uMsg == WM_KEYUP && wParam == VK_RETURN)
                s_suppressChatOpenCharacter = false;
        }

        // Enter opens the connected chat directly. Handle the window message
        // before Skyrim consumes the key, then let OpenChat acquire input and
        // focus the text field on the next ImGui frame. Preserve Alt+Enter for
        // display-mode switching.
        const bool openChatShortcut = !imguiInputActive && uMsg == WM_KEYDOWN && wParam == VK_RETURN && !(GetKeyState(VK_MENU) & 0x8000) && s_pOverlay->GetInGame();
        if (openChatShortcut && pImGuiOverlay->OpenChat())
        {
            // WM_CHAR for the opening Enter may arrive before the next ImGui
            // frame focuses the field. Do not let that character immediately
            // submit and close the freshly opened chat.
            s_suppressChatOpenCharacter = true;
            return 1;
        }

        auto& imgui = World::Get().ctx().at<ImguiService>();

        if (imguiInputActive)
            imgui.WndProcHandler(hwnd, uMsg, wParam, lParam);

        if (uMsg == WM_SETFOCUS || uMsg == WM_KILLFOCUS)
            imgui.OnWindowFocusChanged(uMsg == WM_SETFOCUS);

        if (uMsg == WM_INPUT)
        {
            RAWINPUT input{};
            UINT size = sizeof(input);
            const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));

            if (result != static_cast<UINT>(-1))
            {
                if (imguiInputActive)
                    imgui.RawInputHandler(input);

                if (input.header.dwType == RIM_TYPEKEYBOARD)
                {
                    const auto keyboard = input.data.keyboard;
                    ProcessKeyboard(
                        keyboard.VKey, keyboard.MakeCode, keyboard.Flags & RI_KEY_BREAK ? KEYEVENT_KEYUP : KEYEVENT_KEYDOWN, keyboard.Flags & RI_KEY_E0,
                        keyboard.Flags & RI_KEY_E1);
                }
            }
        }
        else if (uMsg == WM_SETFOCUS && imguiInputActive)
        {
            // Wine can lose raw-input registration while focus changes.
            InputService::RefreshInputState(true);
        }
        else if (uMsg == WM_INPUTLANGCHANGE)
        {
            s_currentACP = GetRealACP();
            spdlog::info("Input language changed, current ACP: {}", s_currentACP);
        }

        // ImGui and Skyrim share the same window. Once the native overlay owns
        // input, do not dispatch the same raw, mouse, or keyboard message to the
        // game's WndProc: Skyrim can otherwise recapture/recenter the cursor and
        // deactivate the currently edited ImGui text field.
        return imguiInputActive && IsImGuiInputMessage(uMsg, wParam) ? 1 : 0;
    }

    const auto pApp = s_pOverlay->GetOverlayApp();
    if (!pApp)
        return 0;

    const auto pClient = pApp->GetClient();
    if (!pClient)
        return 0;

    const auto pRenderer = pClient->GetOverlayRenderHandler();
    if (!pRenderer)
        return 0;

    auto& discord = World::Get().ctx().at<DiscordService>();
    discord.WndProcHandler(hwnd, uMsg, wParam, lParam);

    const bool active = s_pOverlay->GetActive();
    const bool imguiInputActive = active || World::Get().GetDebugService().IsVisible();
    auto& imgui = World::Get().ctx().at<ImguiService>();
    if (imguiInputActive)
        imgui.WndProcHandler(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_SETFOCUS || uMsg == WM_KILLFOCUS)
        imgui.OnWindowFocusChanged(uMsg == WM_SETFOCUS);

    POINT position;

    GetCursorPos(&position);
    ScreenToClient(GetActiveWindow(), &position);

    ProcessMouseMove(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y));

    if (uMsg == WM_INPUT)
    {
        RAWINPUT input;
        UINT size = sizeof(RAWINPUT);

        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));

        if (imguiInputActive)
            imgui.RawInputHandler(input);

        if (input.header.dwType == RIM_TYPEKEYBOARD)
        {
            const auto keyboard = input.data.keyboard;

            ProcessKeyboard(keyboard.VKey, keyboard.MakeCode, keyboard.Flags & RI_KEY_BREAK ? KEYEVENT_KEYUP : KEYEVENT_KEYDOWN, keyboard.Flags & RI_KEY_E0, keyboard.Flags & RI_KEY_E1);
        }
        else if (input.header.dwType == RIM_TYPEMOUSE)
        {
            const auto mouse = input.data.mouse;

            if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
            {
                ProcessMouseWheel(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), (int16_t)mouse.usButtonData);
            }

            if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_LEFT, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_LEFT, false);
            }

            if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_RIGHT, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_RIGHT, false);
            }

            if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_MIDDLE, true);
            }

            if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
            {
                ProcessMouseButton(static_cast<uint16_t>(position.x), static_cast<uint16_t>(position.y), MBT_MIDDLE, false);
            }
        }
    }
    else if (uMsg == WM_CHAR)
    {
        uint16_t scancode = (lParam >> 16) & 0xFF;
        uint16_t virtualKey = static_cast<uint16_t>(wParam);
        if (!IsWindowUnicode(hwnd))
        {
            wchar_t wch;
            ::MultiByteToWideChar(s_currentACP, MB_PRECOMPOSED, reinterpret_cast<char*>(&virtualKey), 2, &wch, sizeof(wchar_t));
            virtualKey = wch;
        }
        ProcessKeyboard(virtualKey, scancode, KEYEVENT_CHAR, false, false);
    }
    // If the player tabs out/in with UI visible, this WndProc doesn't run during mouse or keyboard events.
    // When player tabs in, force the UI state
    else if (uMsg == WM_SETFOCUS && imguiInputActive)
    {
        if (active)
        {
            s_pOverlay->SetActive(true);
            pRenderer->SetCursorVisible(true);
        }

        InputService::RefreshInputState(true);
    }
    else if (uMsg == WM_INPUTLANGCHANGE)
    {
        s_currentACP = GetRealACP();
        spdlog::info("Input language changed, current ACP: {}", s_currentACP);
    }

    return 0;
}

InputService::InputService(OverlayService& aOverlay) noexcept
{
    s_pOverlay = &aOverlay;
    s_currentACP = GetRealACP();
}

InputService::~InputService() noexcept
{
    s_pOverlay = nullptr;
}
