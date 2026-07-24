-- linux_probe: ferramenta de diagnóstico, não faz parte do produto.
-- Deliberadamente sem dependências do STR (nem client, nem TiltedCore, nem CEF):
-- o teste isola a variável "modo de carregamento" e nada mais. Ver README.md.

target("LinuxProbeLoader")
    set_kind("binary")
    set_basename("probe_loader")
    set_group("LinuxProbe")
    set_symbols("debug")
    add_files("ProbeLoader.cpp")
    add_ldflags("/SUBSYSTEM:CONSOLE", "/ENTRY:wmainCRTStartup", { force = true })

target("LinuxProbePayload")
    set_kind("shared")
    set_basename("probe_payload")
    set_group("LinuxProbe")
    set_symbols("debug")
    add_files("ProbePayload.cpp")
