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

struct CEGImageInfo
{
    uint32_t protectedEntryPointRva = 0;
    uint32_t originalEntryPointRva = 0;
    uint32_t textRva = 0;
    uint32_t textFileOffset = 0;
    uint32_t textSize = 0;
    uint64_t preferredImageBase = 0;
};

// Descriptografa o Steam CEG diretamente no buffer do executável e devolve os
// dados necessários para reproduzir a imagem em um processo carregado pelo
// sistema. Imagens sem CEG são aceitas sem modificação; layouts CEG inesperados
// falham explicitamente para nunca executar ou aplicar hooks sobre ciphertext.
CEGDecryptResult DecryptCEGInPlace(uint8_t* apImage, size_t aImageSize, CEGImageInfo& aInfo);
} // namespace steam
