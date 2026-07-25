#include "DisplaySettings.h"

#include "utils/Registry.h"

#include <Commctrl.h>
#include <ShlObj.h>
#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <spdlog/spdlog.h>

namespace launcher::display
{
namespace
{
constexpr wchar_t kSkyrimFolder[] = L"Skyrim Special Edition";
constexpr wchar_t kRegistryPath[] = LR"(Software\TiltedPhoques\TiltedEvolution\Skyrim Special Edition)";
constexpr wchar_t kDisplayModeValue[] = L"DisplayMode";
constexpr wchar_t kShowDisplayPickerValue[] = L"ShowDisplayPicker";

constexpr int kBorderlessRadioId = 4100;
constexpr int kFullscreenRadioId = 4101;
constexpr int kWindowedRadioId = 4102;
constexpr int kCurrentRadioId = 4103;

enum class Mode
{
    kBorderless,
    kFullscreen,
    kWindowed,
    kCurrent,
};

struct Configuration
{
    Mode mode = Mode::kBorderless;
    bool showPicker = true;
};

struct CommandLineOptions
{
    std::optional<Mode> mode;
    bool configure = false;
    bool skipPicker = false;
};

bool IsRunningUnderWine() noexcept
{
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

const wchar_t* ToWideString(Mode aMode) noexcept
{
    switch (aMode)
    {
    case Mode::kFullscreen: return L"fullscreen";
    case Mode::kWindowed: return L"windowed";
    case Mode::kCurrent: return L"current";
    default: return L"borderless";
    }
}

const char* ToString(Mode aMode) noexcept
{
    switch (aMode)
    {
    case Mode::kFullscreen: return "fullscreen";
    case Mode::kWindowed: return "windowed";
    case Mode::kCurrent: return "current Skyrim settings";
    default: return "borderless";
    }
}

std::optional<Mode> ParseMode(std::wstring_view aValue) noexcept
{
    if (aValue == L"borderless")
        return Mode::kBorderless;
    if (aValue == L"fullscreen")
        return Mode::kFullscreen;
    if (aValue == L"windowed")
        return Mode::kWindowed;
    if (aValue == L"current")
        return Mode::kCurrent;

    return std::nullopt;
}

std::optional<Mode> ParseMode(std::string_view aValue) noexcept
{
    if (aValue == "borderless")
        return Mode::kBorderless;
    if (aValue == "fullscreen")
        return Mode::kFullscreen;
    if (aValue == "windowed")
        return Mode::kWindowed;
    if (aValue == "current")
        return Mode::kCurrent;

    return std::nullopt;
}

CommandLineOptions ParseCommandLine(int aArgc, char** apArgv) noexcept
{
    CommandLineOptions options;
    constexpr std::string_view displayModePrefix = "--display-mode=";

    for (int i = 1; i < aArgc; ++i)
    {
        if (!apArgv[i])
            continue;

        const std::string_view argument(apArgv[i]);
        if (argument == "--configure")
            options.configure = true;
        else if (argument == "--skip-launcher-ui")
            options.skipPicker = true;
        else if (argument.starts_with(displayModePrefix))
        {
            const std::string_view value = argument.substr(displayModePrefix.size());
            options.mode = ParseMode(value);
            if (!options.mode)
                spdlog::warn("[display] unknown mode '{}' (expected borderless, fullscreen, windowed, or current)", std::string(value));
        }
    }

    return options;
}

Configuration LoadConfiguration() noexcept
{
    Configuration configuration;

    const std::wstring storedMode = Registry::ReadString<wchar_t>(HKEY_CURRENT_USER, kRegistryPath, kDisplayModeValue);
    if (const auto mode = ParseMode(storedMode))
        configuration.mode = *mode;

    const std::wstring showPicker = Registry::ReadString<wchar_t>(HKEY_CURRENT_USER, kRegistryPath, kShowDisplayPickerValue);
    if (!showPicker.empty())
        configuration.showPicker = showPicker != L"0";

    return configuration;
}

void SaveConfiguration(const Configuration& acConfiguration) noexcept
{
    const bool modeSaved = Registry::WriteString(HKEY_CURRENT_USER, kRegistryPath, kDisplayModeValue, std::wstring(ToWideString(acConfiguration.mode)));
    const bool pickerSaved = Registry::WriteString(HKEY_CURRENT_USER, kRegistryPath, kShowDisplayPickerValue, std::wstring(acConfiguration.showPicker ? L"1" : L"0"));

    if (!modeSaved || !pickerSaved)
        spdlog::warn("[display] could not persist the launcher display preferences");
}

int GetRadioId(Mode aMode) noexcept
{
    switch (aMode)
    {
    case Mode::kFullscreen: return kFullscreenRadioId;
    case Mode::kWindowed: return kWindowedRadioId;
    case Mode::kCurrent: return kCurrentRadioId;
    default: return kBorderlessRadioId;
    }
}

Mode GetModeForRadioId(int aRadioId) noexcept
{
    switch (aRadioId)
    {
    case kFullscreenRadioId: return Mode::kFullscreen;
    case kWindowedRadioId: return Mode::kWindowed;
    case kCurrentRadioId: return Mode::kCurrent;
    default: return Mode::kBorderless;
    }
}

HRESULT CALLBACK DisplayPickerCallback(HWND aWindow, UINT aNotification, WPARAM, LPARAM, LONG_PTR) noexcept
{
    if (aNotification == TDN_CREATED)
        SetWindowPos(aWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    return S_OK;
}

bool ShowDisplayPicker(Configuration& aConfiguration) noexcept
{
    const TASKDIALOG_BUTTON radioButtons[] = {
        {kBorderlessRadioId, L"Borderless window (recommended)\nUses the current Proton desktop resolution and switches cleanly with Alt+Tab."},
        {kFullscreenRadioId, L"Fullscreen\nUses Skyrim's fullscreen mode at the current desktop resolution."},
        {kWindowedRadioId, L"Windowed\nKeeps the resolution already stored in SkyrimPrefs.ini."},
        {kCurrentRadioId, L"Keep current Skyrim settings\nDoes not change SkyrimPrefs.ini."},
    };
    const TASKDIALOG_BUTTON launchButton[] = {
        {IDOK, L"PLAY"},
    };

    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof(dialog);
    dialog.pszWindowTitle = L"Skyrim Together";
    dialog.pszMainInstruction = L"Choose how Skyrim should be displayed";
    dialog.pszContent = L"The selected mode is applied before the game starts. Only display keys are changed; all other Skyrim preferences are preserved.";
    dialog.pszVerificationText = L"Do not show this display picker again";
    dialog.pszFooter = L"Run SkyrimTogether.exe --configure to reopen this picker.";
    dialog.pszMainIcon = TD_INFORMATION_ICON;
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    dialog.cButtons = static_cast<UINT>(std::size(launchButton));
    dialog.pButtons = launchButton;
    dialog.nDefaultButton = IDOK;
    dialog.cRadioButtons = static_cast<UINT>(std::size(radioButtons));
    dialog.pRadioButtons = radioButtons;
    dialog.nDefaultRadioButton = GetRadioId(aConfiguration.mode);
    dialog.pfCallback = DisplayPickerCallback;

    int pressedButton = IDCANCEL;
    int selectedRadio = GetRadioId(aConfiguration.mode);
    BOOL doNotShowAgain = aConfiguration.showPicker ? FALSE : TRUE;
    const HRESULT result = TaskDialogIndirect(&dialog, &pressedButton, &selectedRadio, &doNotShowAgain);
    if (FAILED(result))
    {
        spdlog::warn("[display] display picker failed with HRESULT 0x{:08x}; using the saved mode", static_cast<uint32_t>(result));
        return true;
    }

    if (pressedButton != IDOK)
        return false;

    aConfiguration.mode = GetModeForRadioId(selectedRadio);
    aConfiguration.showPicker = doNotShowAgain == FALSE;
    SaveConfiguration(aConfiguration);
    return true;
}

std::filesystem::path GetSkyrimPrefsPath() noexcept
{
    PWSTR pDocuments = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &pDocuments)) && pDocuments)
    {
        std::filesystem::path path(pDocuments);
        CoTaskMemFree(pDocuments);
        return path / L"My Games" / kSkyrimFolder / L"SkyrimPrefs.ini";
    }

    if (pDocuments)
        CoTaskMemFree(pDocuments);

    const DWORD requiredSize = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (requiredSize == 0)
        return {};

    std::wstring userProfile(requiredSize, L'\0');
    const DWORD written = GetEnvironmentVariableW(L"USERPROFILE", userProfile.data(), requiredSize);
    if (written == 0 || written >= requiredSize)
        return {};

    userProfile.resize(written);
    return std::filesystem::path(userProfile) / L"Documents" / L"My Games" / kSkyrimFolder / L"SkyrimPrefs.ini";
}

bool WriteDisplayValue(const std::filesystem::path& acPrefsPath, const wchar_t* apKey, const std::wstring& acValue) noexcept
{
    const std::wstring prefsPath = acPrefsPath.wstring();
    return WritePrivateProfileStringW(L"Display", apKey, acValue.c_str(), prefsPath.c_str()) != FALSE;
}

bool ApplyDisplayMode(Mode aMode) noexcept
{
    if (aMode == Mode::kCurrent)
    {
        spdlog::info("[display] preserving the current Skyrim display settings");
        return true;
    }

    const std::filesystem::path prefsPath = GetSkyrimPrefsPath();
    if (prefsPath.empty())
    {
        spdlog::error("[display] could not resolve the Documents directory for SkyrimPrefs.ini");
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(prefsPath.parent_path(), error);
    if (error)
    {
        spdlog::error("[display] could not create the Skyrim preferences directory '{}': {}", prefsPath.parent_path().string(), error.message());
        return false;
    }

    const bool prefsExist = std::filesystem::exists(prefsPath, error);
    if (error)
    {
        spdlog::warn("[display] could not inspect the existing Skyrim preferences: {}", error.message());
        error.clear();
    }
    else if (prefsExist)
    {
        std::filesystem::path backupPath = prefsPath;
        backupPath += L".skyrim-together.bak";
        const bool backupExists = std::filesystem::exists(backupPath, error);
        if (error)
        {
            spdlog::warn("[display] could not inspect the Skyrim preferences backup: {}", error.message());
            error.clear();
        }
        else if (!backupExists)
        {
            std::filesystem::copy_file(prefsPath, backupPath, std::filesystem::copy_options::none, error);
            if (error)
                spdlog::warn("[display] could not create the one-time SkyrimPrefs.ini backup: {}", error.message());
            else
                spdlog::info("[display] created preferences backup at {}", backupPath.string());
        }
    }

    const bool borderless = aMode == Mode::kBorderless;
    const bool fullscreen = aMode == Mode::kFullscreen;
    bool succeeded = WriteDisplayValue(prefsPath, L"bBorderless", borderless ? L"1" : L"0");
    succeeded &= WriteDisplayValue(prefsPath, L"bFull Screen", fullscreen ? L"1" : L"0");

    int width = 0;
    int height = 0;
    if (borderless || fullscreen)
    {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
        succeeded &= width > 0 && height > 0;
        if (width > 0 && height > 0)
        {
            succeeded &= WriteDisplayValue(prefsPath, L"iSize W", std::to_wstring(width));
            succeeded &= WriteDisplayValue(prefsPath, L"iSize H", std::to_wstring(height));
        }
    }

    // Flush the Win32 profile cache before Skyrim reads the file.
    const std::wstring widePrefsPath = prefsPath.wstring();
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, widePrefsPath.c_str());

    if (!succeeded)
    {
        spdlog::error("[display] failed to update {}", prefsPath.string());
        return false;
    }

    if (width > 0 && height > 0)
        spdlog::info("[display] applied {} mode at {}x{} in {}", ToString(aMode), width, height, prefsPath.string());
    else
        spdlog::info("[display] applied {} mode in {}", ToString(aMode), prefsPath.string());

    return true;
}
} // namespace

bool Configure(int aArgc, char** apArgv) noexcept
{
    if (!IsRunningUnderWine())
        return true;

    Configuration configuration = LoadConfiguration();
    const CommandLineOptions commandLine = ParseCommandLine(aArgc, apArgv);

    if (commandLine.mode)
    {
        configuration.mode = *commandLine.mode;
        configuration.showPicker = false;
        SaveConfiguration(configuration);
    }

    const bool showPicker = !commandLine.mode && !commandLine.skipPicker && (configuration.showPicker || commandLine.configure);
    if (showPicker && !ShowDisplayPicker(configuration))
        return false;

    if (!ApplyDisplayMode(configuration.mode))
    {
        MessageBoxW(
            nullptr, L"Skyrim Together could not update SkyrimPrefs.ini. The game will continue using its current display settings.", L"Skyrim Together",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }

    return true;
}
} // namespace launcher::display
