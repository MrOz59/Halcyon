#pragma once

namespace launcher::display
{
// Shows the Proton display-mode picker when appropriate, persists the selected
// mode, and applies it to SkyrimPrefs.ini before the game process is created.
// Returns false only when the user cancels the launch.
bool Configure(int aArgc, char** apArgv) noexcept;
} // namespace launcher::display
