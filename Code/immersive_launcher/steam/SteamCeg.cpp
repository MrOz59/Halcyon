
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
        pNtHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
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

    if (!pTextSection || pTextSection->SizeOfRawData < sizeof(SteamStubHeaderV31::CodeSectionStolenData) || pTextSection->SizeOfRawData % AES::BLOCKSIZE != 0 ||
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
    aInfo.preferredImageBase = pNtHeaders->OptionalHeader.ImageBase;

    return CEGDecryptResult::kDecrypted;
}
} // namespace steam
