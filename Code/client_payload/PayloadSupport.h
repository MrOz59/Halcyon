#pragma once

#include <filesystem>

#include <TiltedCore/Stl.hpp>

bool InitializePayloadSupport(const std::filesystem::path& acGamePath, const TiltedPhoques::String& acExeVersion);
