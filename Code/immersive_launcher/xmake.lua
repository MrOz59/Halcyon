local function istable(t) return type(t) == 'table' end

local function build_launcher()
    set_kind("binary")
    set_group("Client")
    set_symbols("debug", "hidden")

    add_ldflags(
        "/FORCE:MULTIPLE",
        "/IGNORE:4254,4006",
        "/DYNAMICBASE:NO",
        "/SAFESEH:NO",
        "/LARGEADDRESSAWARE",
        "/INCREMENTAL:NO",
        "/LAST:.zdata",
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:mainCRTStartup", { force = true })
    add_includedirs(
        ".",
        "../",
        "../../Libraries/")
    add_headerfiles("**.h")
    add_files(
        "**.cpp",
        "launcher.rc")
    add_deps(
        "ImmersiveElf",
        "TiltedReverse",
        "TiltedHooks",
        "TiltedUi",
        "ImGuiImpl",
        "CommonLib")
    add_links("ntdll_x64")
    add_linkdirs(".")
    add_syslinks(
        "user32",
        "comctl32",
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
end

target("SkyrimImmersiveLauncher")
    set_basename("SkyrimTogether")
    add_defines("TARGET_PREFIX=\"st\"")
    add_deps("SkyrimTogetherClient")
    -- O modo externo injeta esta DLL no processo do jogo; garante que ela seja
    -- construída junto com o launcher sem tentar linkar a import library dela.
    add_deps("SkyrimTogetherClientPayload", {inherit = false})
    add_ldflags("/WHOLEARCHIVE:SkyrimTogetherClient", { force = true })
    build_launcher()
