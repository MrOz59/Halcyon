if is_plat("windows") then
    includes("client")
    includes("immersive_elf")
    includes("immersive_launcher")
    includes("client_payload")
    includes("tp_process")
    -- Ferramenta de diagnóstico do port Linux (ver Code/linux_probe/README.md).
    includes("linux_probe")
end

includes("common")
includes("components")
includes("base")
includes("admin_protocol")
includes("server_runner")
includes("server")
includes("encoding")
includes("tests")
