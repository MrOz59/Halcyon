-- DLL injetada no processo do jogo pelo ExternalProcessLauncher (modo externo,
-- necessário sob Wine/Proton). Carrega o mesmo client estático que o launcher usa
-- no modo in-process, então o comportamento dentro do jogo é o mesmo.

target("SkyrimTogetherClientPayload")
    set_kind("shared")
    set_basename("STClientPayload")
    set_group("Client")
    set_symbols("debug", "hidden")

    -- Vários hooks legados do client usam CALL/JMP rel32 diretamente do código
    -- do Skyrim para funções desta DLL. Portanto o payload inteiro precisa ficar
    -- dentro de ±2 GiB da imagem do jogo (0x140000000). Sem isto o Wine pode
    -- aplicar ASLR para uma base baixa, o deslocamento dá wrap em 32 bits e a CPU
    -- salta para uma página inexistente. A base 0x180000000 fica a ~1 GiB do jogo;
    -- /FIXED faz o LoadLibrary falhar caso ela não esteja disponível, em vez de
    -- carregar silenciosamente em uma posição incompatível.
    add_shflags(
        "/BASE:0x180000000",
        "/DYNAMICBASE:NO",
        "/FIXED",
        { force = true })

    -- SkyrimTogetherClient e TiltedHooks são compilados com /GL (LTO). Ao linká-los
    -- numa DLL o linker exige /LTCG e aborta sem ele. Passar "/LTCG" como flag crua
    -- não resolve: o wrapper do linker do xmake detecta o /GL e trata a mensagem de
    -- restart como erro fatal. A policy de LTO integra pelo caminho gerenciado do
    -- xmake e adiciona o /LTCG que o linker pede.
    set_policy("build.optimization.lto", true)

    add_includedirs(
        ".",
        "../",
        "../../Libraries/")

    add_files(
        "ClientPayload.cpp",
        "PayloadSupport.cpp")

    add_deps("SkyrimTogetherClient")

    -- /WHOLEARCHIVE é necessário porque o client registra hooks e inicializadores
    -- via símbolos que ninguém referencia diretamente; sem ele o linker os
    -- descarta.
    add_shflags("/WHOLEARCHIVE:SkyrimTogetherClient", { force = true })

    add_syslinks(
        "user32",
        "shell32",
        "comdlg32",
        "bcrypt",
        "ole32",
        "dxgi",
        "d3d11",
        "gdi32",
        "SetupAPI",
        "Powrprof",
        "Cfgmgr32",
        "Propsys",
        "delayimp")

    add_packages(
        "tiltedcore",
        "spdlog",
        "minhook",
        "hopscotch-map",
        "cryptopp",
        "glm",
        "cef",
        "mem")
