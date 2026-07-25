
target("TPTests")
    set_kind("binary")
    set_group("Tests")
    add_includedirs(
        ".", "../encoding")
    add_headerfiles("**.h")
    add_files("*.cpp")
    add_deps("SkyrimEncoding")
    add_packages(
        "tiltedcore",
        "hopscotch-map",
        "catch2",
        "mimalloc",
        "glm")

    if is_plat("windows") then
        add_files(
            "windows/*.cpp",
            "../immersive_launcher/steam/SteamCeg.cpp")
        add_includedirs("../immersive_launcher")
        add_packages("cryptopp")
    end
