#pragma once

#include <cstddef>
#include <cstdint>

namespace steam
{
enum class CEGDecryptResult
{
    kNotProtected,
    kDecrypted,
    kInvalidImage,
};

enum class CEGRelocateResult
{
    kNotRequired,
    kRelocated,
    kInvalidImage,
    kUnsupportedRelocation,
};

struct CEGImageInfo
{
    uint32_t protectedEntryPointRva = 0;
    uint32_t originalEntryPointRva = 0;
    uint32_t textRva = 0;
    uint32_t textFileOffset = 0;
    uint32_t textSize = 0;
    uint32_t imageSize = 0;
    uint64_t preferredImageBase = 0;
};

// Decrypts Steam CEG directly in the executable buffer and returns the data
// required to reproduce it in a system-loaded process. Images without CEG are
// accepted unchanged; unexpected CEG layouts fail explicitly so encrypted code
// is never executed or hooked.
CEGDecryptResult DecryptCEGInPlace(uint8_t* apImage, size_t aImageSize, CEGImageInfo& aInfo);

// Applies PE base relocations that target the decrypted .text bytes before they
// are copied over a system-loaded image. Relocations outside .text have already
// been handled by the OS loader and are deliberately ignored.
CEGRelocateResult RelocateCEGTextInPlace(uint8_t* apImage, size_t aImageSize, const CEGImageInfo& acInfo, uint64_t aLoadedImageBase, uint32_t& aAppliedRelocations);
} // namespace steam
