-- DLL injetada no processo do jogo pelo ExternalProcessLauncher (modo externo,
-- necessário sob Wine/Proton). Carrega o mesmo client estático que o launcher usa
-- no modo in-process, então o comportamento dentro do jogo é o mesmo.

target("SkyrimTogetherClientPayload")
    set_kind("shared")
    set_basename("STClientPayload")
    set_group("Client")
    set_symbols("debug", "hidden")

    add_includedirs(
        ".",
        "../",
        "../../Libraries/")

    add_files("ClientPayload.cpp")

    add_deps("SkyrimTogetherClient")
    -- O client registra hooks e inicializadores via símbolos que ninguém
    -- referencia diretamente; sem WHOLEARCHIVE o linker os descarta.
    add_ldflags("/WHOLEARCHIVE:SkyrimTogetherClient", { force = true })

    add_ldflags(
        "/FORCE:MULTIPLE",
        "/IGNORE:4254,4006",
        "/DYNAMICBASE:NO",
        "/SAFESEH:NO",
        "/LARGEADDRESSAWARE",
        "/INCREMENTAL:NO",
        -- As dependências (TiltedHooks e cia.) são compiladas com /GL, então o
        -- link precisa de /LTCG; sem isso o linker aborta pedindo a flag.
        "/LTCG", { force = true })

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
