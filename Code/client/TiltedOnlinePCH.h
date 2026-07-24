#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <TiltedCore/Platform.hpp>

#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
#include <type_traits>

// TiltedCore
#include <TiltedCore/StackAllocator.hpp>
#include <TiltedCore/ScratchAllocator.hpp>
#include <TiltedCore/Filesystem.hpp>
#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Outcome.hpp>
#include <TiltedCore/ViewBuffer.hpp>
#include <TiltedCore/Math.hpp>
#include <TiltedCore/TaskQueue.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Initializer.hpp>
#include <TiltedCore/Serialization.hpp>

// TiltedReverse
#include <AutoPtr.hpp>
#include <App.hpp>
#include <FunctionHook.hpp>
#include <Entry.hpp>
#include <Debug.hpp>
#include <ThisCall.hpp>

extern void* RipAllocateN(size_t blockLength);
#define REVERSE_ALLOC_STUB(x) RipAllocateN(x)

// CALL/JMP rel32 só alcança ±2 GiB. O launcher in-process ficava naturalmente
// perto do jogo, mas uma DLL carregada pelo Wine pode estar em qualquer região
// do espaço de endereços. Este relay vive no pool reservado perto do Skyrim e
// salta para o destino final por um endereço absoluto, tornando o hook
// independente da base do módulo que contém a função.
inline void* CreateRel32Relay(const uintptr_t aDestination)
{
    constexpr size_t kRelaySize = 14;
    auto* pRelay = static_cast<uint8_t*>(RipAllocateN(kRelaySize));
    if (!pRelay)
        return nullptr;

    // jmp qword ptr [rip+0]
    static constexpr uint8_t kAbsoluteJump[6]{0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(pRelay, kAbsoluteJump, sizeof(kAbsoluteJump));
    std::memcpy(pRelay + sizeof(kAbsoluteJump), &aDestination, sizeof(aDestination));

    if (!FlushInstructionCache(GetCurrentProcess(), pRelay, kRelaySize))
        return nullptr;

    return pRelay;
}

template <class T> T CreateRel32Relay(T apDestination)
{
    static_assert(std::is_pointer_v<T>);
    return reinterpret_cast<T>(CreateRel32Relay(reinterpret_cast<uintptr_t>(apDestination)));
}

#include <JitAssembly.hpp>

#define SPDLOG_WCHAR_FILENAMES
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include <entt/entt.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <any>
#include <mutex>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>

#include <BuildInfo.h>
#include <Games/Primitives.h>

using TiltedPhoques::Allocator;
using TiltedPhoques::App;
using TiltedPhoques::AutoPtr;
using TiltedPhoques::Buffer;
using TiltedPhoques::List;
using TiltedPhoques::Map;
using TiltedPhoques::Outcome;
using TiltedPhoques::ScopedAllocator;
using TiltedPhoques::ScratchAllocator;
using TiltedPhoques::Set;
using TiltedPhoques::SortedMap;
using TiltedPhoques::StackAllocator;
using TiltedPhoques::String;
using TiltedPhoques::ThisCall;
using TiltedPhoques::UniquePtr;
using TiltedPhoques::Vector;

using namespace std::chrono_literals;

#include "Components.h"

#include <Utils.h>
#include <RTTI.h>
