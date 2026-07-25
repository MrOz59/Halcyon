# Linux / Proton — arquitetura e estado do porte

> Documento técnico do caminho implementado na branch `linux-port`. Para instalação e
> uso, consulte o [README](../README.md).

## Objetivo

Fazer launcher e client do Skyrim Together Reborn funcionarem de forma confiável sob
Wine/Proton, preservando o Windows nativo e a compatibilidade de rede com servidores
vanilla `v1.8.0`.

O client continua sendo código Windows injetado no `SkyrimSE.exe`; convertê-lo para ELF
não faz parte deste porte. Proton fornece a ABI Win32, DX11 e DirectInput necessários.
O servidor dedicado tem um caminho Linux nativo separado.

## Estado validado

- [x] launcher detecta Wine e seleciona a estratégia externa;
- [x] Skyrim SE `1.6.1170` chega ao menu, abre novo jogo e carrega saves;
- [x] Steam CEG é restaurado antes da entrada original;
- [x] payload é injetado e inicializado antes de retomar o jogo;
- [x] hooks relativos fora de alcance usam relay thunks próximos;
- [x] CEF não é inicializado sob Wine;
- [x] overlay ImGui abre com `F2` e libera corretamente o input;
- [x] conexão direta, lista pública, favoritos, jogadores e chat funcionam;
- [x] conexão a servidor vanilla `v1.8.0` confirmada;
- [x] CI produz pacote jogável, símbolos e probe de diagnóstico.

“Validado” descreve o ambiente usado durante o desenvolvimento, não uma garantia para
todas as combinações de distribuição, driver, Proton, mod manager e load order.

## Por que o launcher original falhava

O caminho upstream usa `ExeLoader` para mapear a imagem do jogo dentro do processo do
launcher. Sob Wine, as tabelas de unwind da imagem auto-mapeada não ficam visíveis ao
`RtlVirtualUnwind2`. Exceções que o Windows normalmente desenrolaria terminavam em
crash no início do jogo.

Sob Wine, `GameLauncherFactory` seleciona `ExternalProcessLauncher`:

1. cria `SkyrimSE.exe` como processo real e suspenso;
2. prepara a imagem protegida pelo Steam CEG e restaura a região descriptografada;
3. injeta `STClientPayload.dll`;
4. o payload encontra/aplica os hooks e sinaliza que terminou;
5. o launcher retoma a thread principal na entry point original.

Como o payload pode ficar a mais de ±2 GiB do código do jogo, hooks `rel32` usam relay
thunks alocados perto dos endereços-alvo.

## Por que o CEF foi removido do caminho Proton

O CEF embarcado dispara `0x80000003` dentro de `libcef.dll` no Proton. Foram testados
flags para desligar GPU/rede, SwiftShader, single-process e message pump externo, além
de instrumentação detalhada. Eles mudaram o ponto do crash, mas não produziram uma
inicialização estável.

A decisão final é de plataforma:

- **Wine/Proton:** não inicializa CEF; usa UI ImGui sobre o hook D3D11 existente;
- **Windows nativo:** preserva o overlay CEF upstream.

As chamadas do overlay CEF são protegidas quando seu runtime não existe. Isso evita
que eventos posteriores de input, render ou jogo acessem objetos parcialmente
inicializados. O histórico detalhado está em [cef-proton.md](cef-proton.md).

## UI ImGui

O serviço nativo recebe os mesmos eventos e usa os mesmos serviços de transporte do
cliente:

- conexão direta por hostname, IPv4/IPv6 e senha;
- navegador público via `skyrim-reborn-list.skyrim-together.com/list`;
- busca, filtros, favoritos e validação visual de versão;
- lista de jogadores e níveis;
- chat global em janela separada;
- captura/liberação de DirectInput sincronizada com a visibilidade;
- configurações persistidas em `Data/SkyrimTogetherReborn/native_overlay.json`.

Esse conjunto cobre o caminho de jogo validado. Party/grupo e outros recursos sociais
da interface web original ainda não foram reimplementados.

## Compatibilidade de rede

O commit Git identifica a build nos logs e recursos do executável, mas não deve ser
usado como versão do wire protocol. O fork fixa:

```cpp
#define PROTOCOL_VERSION "v1.8.0"
```

Client e servidor usam esse valor na autenticação e na lista pública. Isso permite
conectar a servidores vanilla `v1.8.0` sem esconder qual commit local está em uso.

O alerta `non_default_install` é apenas informativo. Creations e mods adicionais ainda
podem ser rejeitados pela política do servidor ou causar divergências em jogo.

## Instrumentação

O launcher aceita:

- `--verbose` — log em `debug`;
- `--debug` — log em `trace` e console;
- `--dump-config` — imprime a configuração resolvida sem iniciar o jogo.

Arquivos principais:

- `logs/SkyrimTogether.log`, relativo ao diretório de trabalho do launcher;
- `Skyrim Special Edition/logs/tp_client.log`;
- `Data/SkyrimTogetherReborn/st_client_payload.log`;
- `Data/SkyrimTogetherReborn/st_beginmain_diag.log`.

O cliente registra endpoint, rota usada (IP direto ou resolução de nome), protocolo,
commit, quantidade de mods e fase da autenticação. Timeouts, falha de DNS e problemas
locais de rede são traduzidos para mensagens distintas na UI.

## Build e distribuição

O workflow [linux-port-playable.yml](../.github/workflows/linux-port-playable.yml)
executa o build Windows release porque esse é o formato consumido pelo Proton. O
pacote `str-build` já tem a estrutura correspondente ao diretório `Data`.

Para o servidor nativo há duas rotas upstream:

- Docker: [Dockerfile](../Dockerfile) e [MakeLinux.cmd](../MakeLinux.cmd);
- xmake/Nix: [flake.nix](../flake.nix), seguido de
  `xmake f -p linux -a x64` e `xmake build SkyrimTogetherServer`.

Não confundir o job Windows-para-Proton com a compilação Linux nativa do servidor.

## Próximos passos

- testar mais versões atuais do Proton e diferentes drivers;
- completar os recursos sociais ainda ausentes na UI ImGui;
- automatizar um smoke test do payload além do build/link;
- melhorar integração com Vortex, Lutris, Bottles e Steam Deck;
- revisar a necessidade dos artefatos CEF no pacote exclusivo para Proton.
