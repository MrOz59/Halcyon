#include <catch2/catch.hpp>

#include <steam/SteamCeg.h>

#include <Windows.h>

#include <cstring>
#include <initializer_list>
#include <vector>

namespace
{
constexpr uint64_t kPreferredImageBase = 0x140000000;
constexpr uint64_t kLoadedImageBase = 0x6FFFE7C10000;
constexpr uint64_t kLowerImageBase = 0x130000000;
constexpr uint32_t kTextRva = 0x1000;
constexpr uint32_t kTextFileOffset = 0x200;
constexpr uint32_t kTextSize = 0x200;
constexpr uint32_t kRelocationRva = 0x2000;
constexpr uint32_t kRelocationFileOffset = 0x400;

struct TestImage
{
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x800);
    steam::CEGImageInfo info{};

    TestImage()
    {
        auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());
        pDosHeader->e_magic = IMAGE_DOS_SIGNATURE;
        pDosHeader->e_lfanew = 0x80;

        auto* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes.data() + pDosHeader->e_lfanew);
        pNtHeaders->Signature = IMAGE_NT_SIGNATURE;
        pNtHeaders->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        pNtHeaders->FileHeader.NumberOfSections = 2;
        pNtHeaders->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        pNtHeaders->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        pNtHeaders->OptionalHeader.ImageBase = kPreferredImageBase;
        pNtHeaders->OptionalHeader.SizeOfImage = 0x3000;
        pNtHeaders->OptionalHeader.SizeOfHeaders = kTextFileOffset;
        pNtHeaders->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

        auto* pSections = IMAGE_FIRST_SECTION(pNtHeaders);
        std::memcpy(pSections[0].Name, ".text", 5);
        pSections[0].VirtualAddress = kTextRva;
        pSections[0].Misc.VirtualSize = kTextSize;
        pSections[0].PointerToRawData = kTextFileOffset;
        pSections[0].SizeOfRawData = kTextSize;

        std::memcpy(pSections[1].Name, ".reloc", 6);
        pSections[1].VirtualAddress = kRelocationRva;
        pSections[1].Misc.VirtualSize = 0x200;
        pSections[1].PointerToRawData = kRelocationFileOffset;
        pSections[1].SizeOfRawData = 0x200;

        auto& relocationDirectory = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        relocationDirectory.VirtualAddress = kRelocationRva;

        info.textRva = kTextRva;
        info.textFileOffset = kTextFileOffset;
        info.textSize = kTextSize;
        info.imageSize = pNtHeaders->OptionalHeader.SizeOfImage;
        info.preferredImageBase = kPreferredImageBase;
    }

    void SetRelocationBlock(uint32_t aPageRva, std::initializer_list<uint16_t> aEntries)
    {
        const size_t entryCount = aEntries.size() + aEntries.size() % 2;
        IMAGE_BASE_RELOCATION block{};
        block.VirtualAddress = aPageRva;
        block.SizeOfBlock = static_cast<DWORD>(sizeof(block) + entryCount * sizeof(uint16_t));
        std::memcpy(bytes.data() + kRelocationFileOffset, &block, sizeof(block));

        size_t offset = kRelocationFileOffset + sizeof(block);
        for (const uint16_t entry : aEntries)
        {
            std::memcpy(bytes.data() + offset, &entry, sizeof(entry));
            offset += sizeof(entry);
        }
        if (aEntries.size() != entryCount)
        {
            constexpr uint16_t kAbsolutePadding = 0;
            std::memcpy(bytes.data() + offset, &kAbsolutePadding, sizeof(kAbsolutePadding));
        }

        auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());
        auto* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes.data() + pDosHeader->e_lfanew);
        pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = block.SizeOfBlock;
    }

    void SetTextPointer(uint32_t aOffset, uint64_t aValue) { std::memcpy(bytes.data() + kTextFileOffset + aOffset, &aValue, sizeof(aValue)); }

    [[nodiscard]] uint64_t GetTextPointer(uint32_t aOffset) const
    {
        uint64_t value = 0;
        std::memcpy(&value, bytes.data() + kTextFileOffset + aOffset, sizeof(value));
        return value;
    }
};

constexpr uint16_t RelocationEntry(uint16_t aType, uint16_t aOffset)
{
    return static_cast<uint16_t>((aType << 12) | (aOffset & 0x0FFF));
}
} // namespace

TEST_CASE("CEG text relocation is skipped at the preferred image base", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kTextRva, {RelocationEntry(IMAGE_REL_BASED_DIR64, 0x20)});
    image.SetTextPointer(0x20, kPreferredImageBase + 0x1234);

    uint32_t appliedRelocations = 99;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kPreferredImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kNotRequired);
    REQUIRE(appliedRelocations == 0);
    REQUIRE(image.GetTextPointer(0x20) == kPreferredImageBase + 0x1234);
}

TEST_CASE("CEG DIR64 relocations inside text follow the loaded image base", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kTextRva, {RelocationEntry(IMAGE_REL_BASED_DIR64, 0x20), RelocationEntry(IMAGE_REL_BASED_ABSOLUTE, 0)});
    image.SetTextPointer(0x20, kPreferredImageBase + 0x1234);

    uint32_t appliedRelocations = 0;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kLoadedImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kRelocated);
    REQUIRE(appliedRelocations == 1);
    REQUIRE(image.GetTextPointer(0x20) == kLoadedImageBase + 0x1234);
}

TEST_CASE("CEG DIR64 relocations support a loaded image below its preferred base", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kTextRva, {RelocationEntry(IMAGE_REL_BASED_DIR64, 0x20)});
    image.SetTextPointer(0x20, kPreferredImageBase + 0x1234);

    uint32_t appliedRelocations = 0;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kLowerImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kRelocated);
    REQUIRE(appliedRelocations == 1);
    REQUIRE(image.GetTextPointer(0x20) == kLowerImageBase + 0x1234);
}

TEST_CASE("CEG relocation validates a relocated image with no text fixups", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kRelocationRva, {RelocationEntry(IMAGE_REL_BASED_DIR64, 0x20)});

    uint32_t appliedRelocations = 0;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kLoadedImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kRelocated);
    REQUIRE(appliedRelocations == 0);
}

TEST_CASE("CEG relocation rejects unsupported fixups inside text", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kTextRva, {RelocationEntry(IMAGE_REL_BASED_HIGHLOW, 0x20)});

    uint32_t appliedRelocations = 0;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kLoadedImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kUnsupportedRelocation);
    REQUIRE(appliedRelocations == 0);
}

TEST_CASE("CEG relocation rejects a fixup that crosses the text boundary", "[steam.ceg.relocation]")
{
    TestImage image;
    image.SetRelocationBlock(kTextRva, {RelocationEntry(IMAGE_REL_BASED_DIR64, kTextSize - 4)});

    uint32_t appliedRelocations = 0;
    const auto result = steam::RelocateCEGTextInPlace(image.bytes.data(), image.bytes.size(), image.info, kLoadedImageBase, appliedRelocations);

    REQUIRE(result == steam::CEGRelocateResult::kInvalidImage);
    REQUIRE(appliedRelocations == 0);
}
