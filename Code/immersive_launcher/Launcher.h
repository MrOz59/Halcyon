#pragma once

#include <TiltedCore/Filesystem.hpp>
#include <TiltedCore/Stl.hpp>

namespace launcher
{
namespace fs = std::filesystem;

enum class Result
{
    kSuccess,
    kBadPlatform,
    kBadInstall
};

// stays alive through the entire duration of the game.
struct LaunchContext
{
    fs::path exePath;
    fs::path gamePath;
    TiltedPhoques::String Version;

    void SetLoaded() { isLoaded = true; }   // If loaded, need to spoof GetModuleFileName*(nullptr)
    bool GetLoaded();

  private:
    bool isLoaded{false};
};

LaunchContext* GetLaunchContext();

int StartUp(int argc, char** argv);

bool HandleArguments(int, char**, bool&);

} // namespace launcher
