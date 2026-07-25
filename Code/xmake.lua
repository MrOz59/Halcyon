if is_plat("windows") then
    includes("client")
    includes("immersive_elf")
    includes("immersive_launcher")
    includes("client_payload")
    includes("tp_process")
    if has_config("linux_probe") then
        -- Optional loader/unwind diagnostic. It is not part of the client or
        -- required to run the mod; see Code/linux_probe/README.md.
        includes("linux_probe")
    end
end

includes("common")
includes("components")
includes("base")
includes("admin_protocol")
includes("server_runner")
includes("server")
includes("encoding")
includes("tests")
