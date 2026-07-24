# Linux / Proton — estratégia de portabilidade

> Documento vivo da portabilidade. Complementa [`architecture.md`](architecture.md),
> que descreve o estado atual. Aqui ficam a **estratégia**, as **decisões** e o
> **progresso por fase** do roadmap.

## Princípio norteador

O client do Tilted é **injetado dentro do processo do jogo** (`SkyrimSE.exe`), via um
loader manual de PE (`ExeLoader`). Não é um binário independente. Logo:

- **Não** vamos recompilar o client como ELF nativo Linux.
- **Vamos** fazer o conjunto **launcher + jogo + client rodar sob Proton/Wine**, onde a
  ABI do Windows já existe, e tratar o Linux como plataforma de *hospedagem* oficial.
- O código que **não** exige Windows (config, logging, seleção de path, abrir browser,
  diálogos, integração com Steam/prefix) é o que abstraímos e, quando fizer sentido,
  reimplementamos como ferramenta Linux nativa.

Regra de ouro do roadmap: **Windows continua funcionando sem alterações.** Todo código
específico entra atrás de `#if defined(_WIN32)` / interface de plataforma, nunca
substituindo o caminho Windows.

## Camadas de portabilidade

| Camada | Portável hoje? | Caminho |
|--------|----------------|---------|
| Server | ✅ nativo | Consolidar build/CI/pacotes (Fases 6–7) |
| Código compartilhado (common/encoding/components/base) | ✅ maioria | Guardar Win32 restante atrás de `#ifdef` |
| Launcher (config, logs, paths, UI/diálogos) | ⚠️ parcial | Abstrair via `IPlatform` (Fase 2) |
| Launcher (ExeLoader, usvfs, script_extender) | ❌ Windows/PE | Roda sob Proton; não portar nativamente |
| Client (hooks no jogo, DX11, CEF) | ❌ Windows | Roda sob Proton |
| tp_process (CEF) | ❌ Windows | Roda sob Proton; avaliar CEF Linux só se desacoplar UI |

## Progresso por fase

- [x] **Fase 0 — Engenharia reversa.** Arquitetura documentada em
  [`architecture.md`](architecture.md). Descoberta-chave: launcher carrega o jogo
  **in-process** (ExeLoader), não via `CreateProcess`.
- [~] **Fase 1 — Instrumentação.** Flags `--verbose` / `--debug` / `--dump-config` e
  logging estruturado no launcher, antes de qualquer refactor. *(em andamento)*
- [ ] **Fase 2 — Abstração de plataforma.** Interface `IPlatform`
  (`LaunchProcess`/`OpenBrowser`/`GetKnownFolder`/`ShowMessage`/`GetExecutableDirectory`)
  com `PlatformWindows` e `PlatformLinux`.
- [ ] **Fase 3 — Desacoplar o launcher.** Lógica de boot numa lib compartilhada
  (bootstrap), consumível por GUI e CLI.
- [ ] **Fase 4 — Remover dependência do CEF.** Backends CLI/GTK/Qt sobre o mesmo core.
- [ ] **Fase 5 — Avaliar tp_process sob Proton.** Testar o que roda direto, o que precisa
  de adaptação e o que teria de ser reimplementado.
- [ ] **Fase 6 — CI Linux.** Ubuntu / Arch / Fedora / Steam Runtime.
- [ ] **Fase 7 — Distribuição.** AppImage / Flatpak / tar.gz.
- [ ] **Fase 8 — Gerenciadores de mods.** Vortex Linux / Steam / Lutris / Heroic / Bottles / PortProton.
- [ ] **Fase 9 — Melhorias Linux.** Detecção de prefix/Steam Library, múltiplos prefixes,
  MangoHud, Gamescope, Wayland, crash handler via coredumpctl.

## Servidor nativo no Linux (o caminho de menor risco)

Já suportado. Duas rotas:

- **Docker** (como o CI oficial faz): ver [`Dockerfile`](../Dockerfile) e
  [`MakeLinux.cmd`](../MakeLinux.cmd). Produz `SkyrimTogetherServer` e `libSTServer.so`.
- **Nativo via xmake** (dev local): ver [`flake.nix`](../flake.nix) — provê `gcc14`,
  `xmake`, `cmake`. `xmake f -p linux -a x64` + `xmake build SkyrimTogetherServer`.

Alvos incluídos no Linux por [`Code/xmake.lua`](../Code/xmake.lua): `server`,
`server_runner`, `common`, `components`, `base`, `admin_protocol`, `encoding`, `tests`.

## Rodar o client sob Proton (esboço — a validar na Fase 5)

1. Instalar o mod normalmente no prefixo Proton do Skyrim SE (SKSE + Address Library).
2. Fazer o `SkyrimTogether.exe` (launcher) ser o executável lançado pelo Steam/Proton no
   lugar do jogo (ele carrega o jogo in-process).
3. Verificar: preload de DLLs de sistema, usvfs, CEF (tp_process), e a overlay DX11.
4. Documentar cada ponto de falha aqui conforme testado.

> Ainda não validado nesta máquina. A instrumentação da Fase 1 existe justamente para
> tornar esses pontos de falha visíveis nos logs.

## Instrumentação (Fase 1) — como usar

O launcher aceita flags de diagnóstico (Windows e futuramente Linux):

- `--verbose` — eleva o nível de log para `debug`.
- `--debug` — eleva para `trace` (máximo detalhe) e força console.
- `--dump-config` — imprime a configuração de inicialização resolvida (caminhos,
  versão, plataforma, args) e sai sem iniciar o jogo.

Logs são escritos em `logs/` (sink rotativo) e no console, seguindo o mesmo padrão
spdlog do servidor. Variável de ambiente equivalente: `TE_LOG_LEVEL`.
