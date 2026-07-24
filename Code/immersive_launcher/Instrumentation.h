#pragma once

// Fase 1 do roadmap de portabilidade Linux: instrumentação de diagnóstico.
//
// Fornece flags de linha de comando (--verbose / --debug / --dump-config) e a
// configuração de logging do launcher. Mantido separado da lógica de plataforma
// para que possa ser reutilizado por qualquer front-end (GUI, CLI) no futuro.
//
// A maior parte deste módulo é intencionalmente agnóstica de plataforma; apenas
// a criação do console de depuração usa API do Windows, guardada por #ifdef.

#include <TiltedCore/Stl.hpp>

namespace launcher::instrumentation
{
// Opções de diagnóstico resolvidas a partir dos argumentos e do ambiente.
struct Options
{
    // --verbose: eleva o log para "debug".
    bool verbose = false;
    // --debug: eleva o log para "trace" e força a abertura do console.
    bool debug = false;
    // --dump-config: imprime a configuração resolvida e encerra sem iniciar o jogo.
    bool dumpConfig = false;
};

// Faz o parse dos argumentos de diagnóstico. Não consome nem valida os demais
// argumentos do launcher (ex.: --exePath) — apenas observa os relevantes aqui.
// Também respeita a variável de ambiente TE_LOG_LEVEL (spdlog level string).
Options ParseOptions(int aArgc, char** aArgv);

// Inicializa o logger padrão do spdlog (sink rotativo em logs/ + console),
// aplicando o nível derivado das opções / do ambiente. Idempotente.
void SetupLogging(const Options& aOptions);

// Imprime, via logger, a configuração de inicialização resolvida.
// Chamado sempre; com --dump-config o launcher encerra logo em seguida.
void DumpConfig(const Options& aOptions, const TiltedPhoques::String& aExePath, const TiltedPhoques::String& aGamePath, const TiltedPhoques::String& aVersion);
} // namespace launcher::instrumentation
