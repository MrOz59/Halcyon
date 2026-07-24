-- DLL injetada no processo do jogo pelo ExternalProcessLauncher (modo externo,
-- necessário sob Wine/Proton). Carrega o mesmo client estático que o launcher usa
-- no modo in-process, então o comportamento dentro do jogo é o mesmo.

target("SkyrimTogetherClientPayload")
    set_kind("shared")
    set_basename("STClientPayload")
    set_group("Client")
    set_symbols("debug", "hidden")

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

    add_files("ClientPayload.cpp")

    add_deps("SkyrimTogetherClient")

    -- /WHOLEARCHIVE é necessário porque o client registra hooks e inicializadores
    -- via símbolos que ninguém referencia diretamente; sem ele o linker os
    -- descarta.
    add_ldflags("/WHOLEARCHIVE:SkyrimTogetherClient")

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
