#include "PayloadSupport.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

#include <immersive_launcher/Launcher.h>

namespace
{
// The client emits rel32 jumps between Skyrim code and generated Xbyak stubs.
// Keep the pool comfortably inside the signed 32-bit displacement range of the
// whole main image. The in-process loader gets this guarantee from `highrip`;
// the external payload has to reserve an equivalent region in the game process.
constexpr size_t kRipPoolSize = 0x100000;
constexpr uintptr_t kRipSearchDistance = 0x70000000;
constexpr size_t kStubAlignment = 16;

launcher::LaunchContext g_payloadContext;
uint8_t* g_ripPool = nullptr;
size_t g_ripPoolOffset = 0;
std::mutex g_ripPoolMutex;

uintptr_t AlignDown(const uintptr_t aValue, const uintptr_t aAlignment)
{
    return aValue & ~(aAlignment - 1);
}

uintptr_t AlignUp(const uintptr_t aValue, const uintptr_t aAlignment)
{
    return AlignDown(aValue + aAlignment - 1, aAlignment);
}

uint8_t* TryAllocateRipPool(const uintptr_t aAddress)
{
    return static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(aAddress), kRipPoolSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
}

uint8_t* AllocateRipPool()
{
    const auto mainModule = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if (!mainModule)
        return nullptr;

    const auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(mainModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    const auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(mainModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);

    const uintptr_t granularity = systemInfo.dwAllocationGranularity;
    const uintptr_t moduleStart = reinterpret_cast<uintptr_t>(mainModule);
    const uintptr_t moduleEnd = moduleStart + ntHeaders->OptionalHeader.SizeOfImage;
    if (moduleStart <= kRipPoolSize)
        return nullptr;

    const uintptr_t upwardStart = AlignUp(moduleEnd, granularity);
    const uintptr_t downwardStart = AlignDown(moduleStart - kRipPoolSize, granularity);

    // Prefer addresses immediately after the image, then immediately before it.
    // VirtualAlloc with an explicit, allocation-granularity-aligned address only
    // succeeds when the entire requested range is free.
    for (uintptr_t distance = 0; distance <= kRipSearchDistance; distance += granularity)
    {
        if (upwardStart <= std::numeric_limits<uintptr_t>::max() - distance)
        {
            const uintptr_t candidate = upwardStart + distance;
            if (candidate - moduleStart <= kRipSearchDistance)
            {
                if (auto* pool = TryAllocateRipPool(candidate))
                    return pool;
            }
        }

        if (downwardStart >= distance)
        {
            const uintptr_t candidate = downwardStart - distance;
            if (moduleEnd - candidate <= kRipSearchDistance)
            {
                if (auto* pool = TryAllocateRipPool(candidate))
                    return pool;
            }
        }
    }

    return nullptr;
}
} // namespace

namespace launcher
{
LaunchContext* GetLaunchContext()
{
    return &g_payloadContext;
}

bool LaunchContext::GetLoaded()
{
    // In external mode the real game executable is the process main module, so
    // the filename-spoofing path used by the in-process loader is never needed.
    return false;
}
} // namespace launcher

bool InitializePayloadSupport(const std::filesystem::path& acGamePath, const TiltedPhoques::String& acExeVersion)
{
    g_payloadContext.gamePath = acGamePath;
    g_payloadContext.Version = acExeVersion;

    wchar_t exePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) != 0)
        g_payloadContext.exePath = exePath;

    std::scoped_lock lock(g_ripPoolMutex);
    if (!g_ripPool)
        g_ripPool = AllocateRipPool();
    return g_ripPool != nullptr;
}

// Deliberately in no namespace: TiltedOnlinePCH.h declares this symbol for the
// custom allocator used by TiltedReverse::CodeGenerator.
void* RipAllocateN(const size_t aBlockLength)
{
    if (aBlockLength == 0 || aBlockLength > std::numeric_limits<size_t>::max() - (kStubAlignment - 1))
    {
        return nullptr;
    }

    const size_t alignedLength = (aBlockLength + (kStubAlignment - 1)) & ~(kStubAlignment - 1);

    std::scoped_lock lock(g_ripPoolMutex);

    if (!g_ripPool)
        g_ripPool = AllocateRipPool();

    if (!g_ripPool || alignedLength > kRipPoolSize - g_ripPoolOffset)
        return nullptr;

    auto* allocation = g_ripPool + g_ripPoolOffset;
    g_ripPoolOffset += alignedLength;
    std::memset(allocation, 0xCC, alignedLength);
    return allocation;
}
