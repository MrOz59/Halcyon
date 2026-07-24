// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.
#pragma once

#include <filesystem>

#include <TiltedCore/Stl.hpp>

namespace launcher::launch
{
// Como o SkyrimSE.exe é carregado e como o client entra nele.
//
// Existem duas estratégias porque o mapeamento manual de PE (kInProcess) não é
// utilizável sob Wine/Proton: as unwind tables de um módulo auto-mapeado não são
// visíveis para o RtlVirtualUnwind2. RtlAddFunctionTable registra uma tabela
// DINÂMICA, que é a segunda escolha do lookup; a primeira é a tabela do módulo
// carregado pelo loader (LdrpInvertedFunctionTable), nunca populada para uma
// região que o loader não conhece como imagem. O próprio ExeLoader.cpp reconhece
// isso em comentário ("we have no influence on those"), e CrtStartupHooks.cpp
// engole a exceção 0x406D1388 como paliativo "till we add SEH table support".
//
// Medido sob GE-Proton11-1: com kExternalProcess o jogo sobrevive à 0x406D1388 e
// segue normalmente; com kInProcess o unwind entra na base do jogo, segue RIPs
// inválidos e o processo morre com SIGFPE. Ver Code/linux_probe/README.md.
enum class Strategy
{
    // Mapeia o PE à mão no processo do launcher e salta para o entry point.
    // Caminho histórico e o padrão no Windows.
    kInProcess,

    // Cria o SkyrimSE.exe como processo real e suspenso, restaura o .text CEG,
    // injeta o client antes do entry point real e resume. Necessário sob Proton.
    kExternalProcess,
};

// Descreve o que precisa ser levado para dentro do jogo, seja no processo atual
// ou num processo separado.
struct LaunchRequest
{
    std::filesystem::path exePath;    // SkyrimSE.exe
    std::filesystem::path gamePath;   // diretório de instalação do jogo
    TiltedPhoques::String exeVersion; // usado pelo Address Library
};

class IGameLauncher
{
public:
    virtual ~IGameLauncher() = default;

    // Prepara o jogo sem executá-lo (mapear ou criar suspenso). Após esta chamada
    // o client ainda não rodou.
    virtual bool Prepare(const LaunchRequest& acRequest) = 0;

    // Entrega o controle ao jogo. Só retorna quando o jogo termina; o código de
    // saída fica em GetExitCode().
    virtual bool Run() = 0;

    virtual uint32_t GetExitCode() const = 0;

    virtual Strategy GetStrategy() const = 0;
};

// Escolhe a estratégia. Sob Wine/Proton o mapeamento manual não funciona, então o
// padrão passa a ser kExternalProcess; no Windows nada muda.
Strategy SelectDefaultStrategy() noexcept;

// Permite forçar a estratégia por linha de comando (--launch-mode=inprocess|external),
// para comparar os dois caminhos no mesmo binário durante o diagnóstico.
Strategy ParseStrategyOverride(int aArgc, char** apArgv, bool& aOverridden) noexcept;

TiltedPhoques::UniquePtr<IGameLauncher> CreateGameLauncher(Strategy aStrategy);

const char* ToString(Strategy aStrategy) noexcept;

} // namespace launcher::launch
