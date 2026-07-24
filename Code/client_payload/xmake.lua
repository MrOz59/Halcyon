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

    -- Apenas o essencial em ldflags. As libs do projeto são compiladas com /GL no
    -- modo release, e o xmake injeta o /LTCG correspondente por conta própria —
    -- mas um add_ldflags com force=true substitui esse conjunto gerenciado e
    -- apaga o /LTCG, fazendo o linker abortar. É por isso que ImmersiveElf (a
    -- outra DLL do projeto) linka sem declarar ldflags nenhuma.
    --
    -- /WHOLEARCHIVE é necessário porque o client registra hooks e inicializadores
    -- via símbolos que ninguém referencia diretamente; sem ele o linker os
    -- descarta. Sem force, para somar às flags do xmake em vez de trocá-las.
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
