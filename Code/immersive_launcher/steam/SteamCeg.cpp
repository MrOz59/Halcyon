
#include "SteamCeg.h"
#include "SteamCrypto.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <Windows.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

namespace steam
{
using namespace CryptoPP;

namespace
{
uint32_t SteamXor(uint8_t* apData, uint32_t aSize, uint32_t aKey)
{
    // Decode the data..
    for (size_t x = 4; x < aSize; x += 4)
    {
        uint32_t value = 0;
        std::memcpy(&value, apData + x, sizeof(value));

        const uint32_t decoded = value ^ aKey;
        std::memcpy(apData + x, &decoded, sizeof(decoded));
        aKey = value;
    }

    return aKey;
}

bool FitsInImage(size_t aOffset, size_t aLength, size_t aImageSize)
{
    return aOffset <= aImageSize && aLength <= aImageSize - aOffset;
}

bool RvaToFileOffset(const IMAGE_NT_HEADERS64& acNtHeaders, uint32_t aRva, size_t aLength, size_t aImageSize, uint32_t& aFileOffset)
{
    if (aRva < acNtHeaders.OptionalHeader.SizeOfHeaders)
    {
        if (!FitsInImage(aRva, aLength, aImageSize))
            return false;

        aFileOffset = aRva;
        return true;
    }

    const auto* pSection = IMAGE_FIRST_SECTION(&acNtHeaders);
    for (uint16_t i = 0; i < acNtHeaders.FileHeader.NumberOfSections; ++i)
    {
        const uint32_t sectionRva = pSection[i].VirtualAddress;
        const uint32_t sectionSize = std::max(pSection[i].Misc.VirtualSize, pSection[i].SizeOfRawData);
        if (aRva < sectionRva || static_cast<uint64_t>(aRva) + aLength > static_cast<uint64_t>(sectionRva) + sectionSize)
            continue;

        const uint64_t delta = aRva - sectionRva;
        if (delta + aLength > pSection[i].SizeOfRawData)
            return false;

        const uint64_t offset = static_cast<uint64_t>(pSection[i].PointerToRawData) + delta;
        if (offset > std::numeric_limits<uint32_t>::max() || !FitsInImage(static_cast<size_t>(offset), aLength, aImageSize))
            return false;

        aFileOffset = static_cast<uint32_t>(offset);
        return true;
    }

    return false;
}
} // namespace

CEGDecryptResult DecryptCEGInPlace(uint8_t* apImage, size_t aImageSize, CEGImageInfo& aInfo)
{
    aInfo = {};

    if (!apImage || !FitsInImage(0, sizeof(IMAGE_DOS_HEADER), aImageSize))
        return CEGDecryptResult::kInvalidImage;

    const auto* pDosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(apImage);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || pDosHeader->e_lfanew < 0 || !FitsInImage(static_cast<size_t>(pDosHeader->e_lfanew), sizeof(IMAGE_NT_HEADERS64), aImageSize))
    {
        return CEGDecryptResult::kInvalidImage;
    }

    auto* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(apImage + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE || pNtHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        pNtHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || pNtHeaders->FileHeader.NumberOfSections == 0 ||
        pNtHeaders->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
    {
        return CEGDecryptResult::kInvalidImage;
    }

    const size_t sectionTableOffset = static_cast<size_t>(pDosHeader->e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + pNtHeaders->FileHeader.SizeOfOptionalHeader;
    const size_t sectionTableSize = static_cast<size_t>(pNtHeaders->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (!FitsInImage(sectionTableOffset, sectionTableSize, aImageSize))
        return CEGDecryptResult::kInvalidImage;

    const uint32_t protectedEntryPoint = pNtHeaders->OptionalHeader.AddressOfEntryPoint;
    uint32_t entryPointOffset = 0;
    if (!RvaToFileOffset(*pNtHeaders, protectedEntryPoint, 5, aImageSize, entryPointOffset))
        return CEGDecryptResult::kInvalidImage;

    static constexpr uint8_t kCegEntrySignature[5]{0xE8, 0x00, 0x00, 0x00, 0x00};
    if (std::memcmp(apImage + entryPointOffset, kCegEntrySignature, sizeof(kCegEntrySignature)) != 0)
        return CEGDecryptResult::kNotProtected;

    const auto* pSections = IMAGE_FIRST_SECTION(pNtHeaders);
    const IMAGE_SECTION_HEADER* pTextSection = nullptr;
    for (uint16_t i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i)
    {
        if (std::memcmp(pSections[i].Name, ".text", 5) == 0 && (pSections[i].Characteristics & IMAGE_SCN_CNT_CODE) != 0)
        {
            pTextSection = &pSections[i];
            break;
        }
    }

    const uint32_t imageSize = pNtHeaders->OptionalHeader.SizeOfImage;
    const uint64_t mappedTextEnd = pTextSection ? static_cast<uint64_t>(pTextSection->VirtualAddress) + pTextSection->SizeOfRawData : 0;
    if (!pTextSection || imageSize == 0 || pNtHeaders->OptionalHeader.ImageBase == 0 || pNtHeaders->OptionalHeader.ImageBase > std::numeric_limits<uint64_t>::max() - imageSize ||
        mappedTextEnd > imageSize || pTextSection->SizeOfRawData < sizeof(SteamStubHeaderV31::CodeSectionStolenData) || pTextSection->SizeOfRawData % AES::BLOCKSIZE != 0 ||
        !FitsInImage(pTextSection->PointerToRawData, pTextSection->SizeOfRawData, aImageSize) || entryPointOffset < sizeof(SteamStubHeaderV31))
    {
        return CEGDecryptResult::kInvalidImage;
    }

    SteamStubHeaderV31 stub{};
    std::memcpy(&stub, apImage + entryPointOffset - sizeof(stub), sizeof(stub));
    SteamXor(reinterpret_cast<uint8_t*>(&stub), sizeof(stub), stub.XorKey);

    const auto& lastSection = pSections[pNtHeaders->FileHeader.NumberOfSections - 1];
    if (stub.Signature != 0xC0DEC0DF || stub.OriginalEntryPoint > std::numeric_limits<uint32_t>::max() || stub.ImageBase != pNtHeaders->OptionalHeader.ImageBase ||
        stub.AddressOfEntryPoint != protectedEntryPoint || stub.CodeSectionVirtualAddress != pTextSection->VirtualAddress ||
        stub.CodeSectionRawSize != pTextSection->SizeOfRawData || std::memcmp(lastSection.Name, ".bind", 5) != 0)
        return CEGDecryptResult::kInvalidImage;

    const uint32_t originalEntryPoint = static_cast<uint32_t>(stub.OriginalEntryPoint);
    const uint64_t textEnd = static_cast<uint64_t>(pTextSection->VirtualAddress) + pTextSection->Misc.VirtualSize;
    if (originalEntryPoint < pTextSection->VirtualAddress || originalEntryPoint >= textEnd)
        return CEGDecryptResult::kInvalidImage;

    ECB_Mode<AES>::Decryption ecbDec(stub.AES_Key, sizeof(stub.AES_Key));
    ecbDec.ProcessData(stub.AES_IV, stub.AES_IV, sizeof(stub.AES_IV));

    constexpr size_t kStolenCodeSize = sizeof(SteamStubHeaderV31::CodeSectionStolenData);
    uint8_t* pText = apImage + pTextSection->PointerToRawData;
    std::memmove(pText + kStolenCodeSize, pText, pTextSection->SizeOfRawData - kStolenCodeSize);
    std::memcpy(pText, stub.CodeSectionStolenData, kStolenCodeSize);

    CBC_Mode<AES>::Decryption cbcDec(stub.AES_Key, sizeof(stub.AES_Key), stub.AES_IV);
    cbcDec.ProcessData(pText, pText, pTextSection->SizeOfRawData);

    aInfo.protectedEntryPointRva = protectedEntryPoint;
    aInfo.originalEntryPointRva = originalEntryPoint;
    aInfo.textRva = pTextSection->VirtualAddress;
    aInfo.textFileOffset = pTextSection->PointerToRawData;
    aInfo.textSize = pTextSection->SizeOfRawData;
    aInfo.imageSize = imageSize;
    aInfo.preferredImageBase = pNtHeaders->OptionalHeader.ImageBase;

    return CEGDecryptResult::kDecrypted;
}

CEGRelocateResult RelocateCEGTextInPlace(uint8_t* apImage, size_t aImageSize, const CEGImageInfo& acInfo, uint64_t aLoadedImageBase, uint32_t& aAppliedRelocations)
{
    aAppliedRelocations = 0;

    if (!apImage || acInfo.preferredImageBase == 0 || aLoadedImageBase == 0 || acInfo.imageSize == 0 || acInfo.textSize == 0 ||
        !FitsInImage(acInfo.textFileOffset, acInfo.textSize, aImageSize) || acInfo.preferredImageBase > std::numeric_limits<uint64_t>::max() - acInfo.imageSize ||
        aLoadedImageBase > std::numeric_limits<uint64_t>::max() - acInfo.imageSize)
    {
        return CEGRelocateResult::kInvalidImage;
    }

    if (aLoadedImageBase == acInfo.preferredImageBase)
        return CEGRelocateResult::kNotRequired;

    if (!FitsInImage(0, sizeof(IMAGE_DOS_HEADER), aImageSize))
        return CEGRelocateResult::kInvalidImage;

    const auto* pDosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(apImage);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || pDosHeader->e_lfanew < 0 || !FitsInImage(static_cast<size_t>(pDosHeader->e_lfanew), sizeof(IMAGE_NT_HEADERS64), aImageSize))
    {
        return CEGRelocateResult::kInvalidImage;
    }

    const auto* pNtHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(apImage + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE || pNtHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        pNtHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || pNtHeaders->FileHeader.NumberOfSections == 0 ||
        pNtHeaders->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || pNtHeaders->OptionalHeader.ImageBase != acInfo.preferredImageBase)
    {
        return CEGRelocateResult::kInvalidImage;
    }

    const size_t sectionTableOffset = static_cast<size_t>(pDosHeader->e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + pNtHeaders->FileHeader.SizeOfOptionalHeader;
    const size_t sectionTableSize = static_cast<size_t>(pNtHeaders->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (!FitsInImage(sectionTableOffset, sectionTableSize, aImageSize) || pNtHeaders->OptionalHeader.SizeOfImage != acInfo.imageSize ||
        pNtHeaders->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC)
    {
        return CEGRelocateResult::kInvalidImage;
    }

    const auto& relocationDirectory = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocationDirectory.VirtualAddress == 0 || relocationDirectory.Size < sizeof(IMAGE_BASE_RELOCATION) ||
        static_cast<uint64_t>(relocationDirectory.VirtualAddress) + relocationDirectory.Size > acInfo.imageSize)
        return CEGRelocateResult::kInvalidImage;

    uint32_t relocationFileOffset = 0;
    if (!RvaToFileOffset(*pNtHeaders, relocationDirectory.VirtualAddress, relocationDirectory.Size, aImageSize, relocationFileOffset))
        return CEGRelocateResult::kInvalidImage;

    const uint64_t textStart = acInfo.textRva;
    const uint64_t textEnd = textStart + acInfo.textSize;
    if (textEnd > acInfo.imageSize)
        return CEGRelocateResult::kInvalidImage;

    const uint64_t relocationDelta = aLoadedImageBase - acInfo.preferredImageBase;
    size_t cursor = relocationFileOffset;
    const size_t directoryEnd = cursor + relocationDirectory.Size;

    while (cursor < directoryEnd)
    {
        if (directoryEnd - cursor < sizeof(IMAGE_BASE_RELOCATION))
            return CEGRelocateResult::kInvalidImage;

        IMAGE_BASE_RELOCATION block{};
        std::memcpy(&block, apImage + cursor, sizeof(block));

        // A zero block is permitted as alignment padding at the end of the
        // directory, but non-zero data after it would make the table ambiguous.
        if (block.VirtualAddress == 0 && block.SizeOfBlock == 0)
        {
            if (std::any_of(apImage + cursor, apImage + directoryEnd, [](uint8_t aValue) { return aValue != 0; }))
                return CEGRelocateResult::kInvalidImage;
            break;
        }

        if (block.SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) || block.SizeOfBlock > directoryEnd - cursor ||
            (block.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) % sizeof(uint16_t) != 0)
        {
            return CEGRelocateResult::kInvalidImage;
        }

        const size_t entryCount = (block.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        const size_t entriesOffset = cursor + sizeof(IMAGE_BASE_RELOCATION);

        for (size_t i = 0; i < entryCount; ++i)
        {
            uint16_t entry = 0;
            std::memcpy(&entry, apImage + entriesOffset + i * sizeof(entry), sizeof(entry));

            const uint16_t type = entry >> 12;
            if (type == IMAGE_REL_BASED_ABSOLUTE)
                continue;

            const uint64_t targetRva = static_cast<uint64_t>(block.VirtualAddress) + (entry & 0x0FFF);
            if (targetRva < textStart || targetRva >= textEnd)
                continue;

            if (type != IMAGE_REL_BASED_DIR64)
                return CEGRelocateResult::kUnsupportedRelocation;

            if (targetRva + sizeof(uint64_t) > textEnd)
                return CEGRelocateResult::kInvalidImage;

            const uint64_t textOffset = targetRva - textStart;
            if (textOffset > std::numeric_limits<size_t>::max() - acInfo.textFileOffset)
                return CEGRelocateResult::kInvalidImage;

            const size_t targetFileOffset = static_cast<size_t>(acInfo.textFileOffset + textOffset);
            if (!FitsInImage(targetFileOffset, sizeof(uint64_t), aImageSize))
            {
                return CEGRelocateResult::kInvalidImage;
            }

            uint64_t value = 0;
            std::memcpy(&value, apImage + targetFileOffset, sizeof(value));
            value += relocationDelta;
            std::memcpy(apImage + targetFileOffset, &value, sizeof(value));
            ++aAppliedRelocations;
        }

        cursor += block.SizeOfBlock;
    }

    return CEGRelocateResult::kRelocated;
}
} // namespace steam
