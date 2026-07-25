# Skyrim Together Reborn — fork para Linux/Proton

[![Build para Proton](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml/badge.svg?branch=linux-port)](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml)
[![Licença GPLv3](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)

Este fork adapta o cliente do **Skyrim Together Reborn** para executar o Skyrim Special
Edition pelo Proton. O objetivo é manter o protocolo dos servidores oficiais e trocar
somente os caminhos que não funcionam de forma confiável sob Wine.

> Este é um fork comunitário e experimental, não uma versão oficial do Skyrim
> Together. O projeto original, sua documentação e seus canais de suporte estão em
> [TiltedEvolution](https://github.com/tiltedphoques/TiltedEvolution) e na
> [wiki oficial](https://wiki.tiltedphoques.com/tilted-online/).

## Estado atual

O caminho principal já foi validado no ambiente de desenvolvimento:

- Skyrim SE `1.6.1170` inicia pelo launcher dentro do Proton;
- novo jogo e saves existentes carregam;
- a interface abre e fecha com `F2`;
- conexão direta e navegador de servidores públicos funcionam;
- conexão com servidores vanilla do protocolo `v1.8.0` foi confirmada;
- lista de jogadores e chat global funcionam, com o chat em uma janela separada.

O cliente **não é um executável Linux nativo**. Launcher, jogo e payload continuam
sendo binários Windows PE e rodam no mesmo prefixo Proton. O servidor dedicado já
possui seu caminho Linux nativo no projeto upstream.

## Instalação

1. Abra o workflow
   [Linux-port playable build](https://github.com/MrOz59/TiltedEvolution-linux/actions/workflows/linux-port-playable.yml)
   e baixe o artefato `SkyrimTogether-linux-port (linux-port-<commit>)` da execução
   bem-sucedida mais recente.
2. Instale o conteúdo do artefato como um mod cujo diretório raiz corresponde ao
   `Data` do Skyrim. Isso pode ser feito pelo Vortex ou manualmente. Ao final, deve
   existir:

   ```text
   Skyrim Special Edition/
   └── Data/
       ├── SkyrimTogether.esp
       ├── SkyrimTogetherRebornBehaviors/
       ├── scripts/
       └── SkyrimTogetherReborn/
           └── SkyrimTogether.exe
   ```

3. Ative `SkyrimTogether.esp` e use o mesmo prefixo/versão do Proton configurado para
   o Skyrim.
4. Cadastre `Data/SkyrimTogetherReborn/SkyrimTogether.exe` como ferramenta no
   gerenciador de mods. Passe `--exePath` quando o launcher não localizar o jogo
   sozinho, por exemplo:

   ```text
   --exePath "Z:\caminho\para\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe"
   ```

5. Inicie essa ferramenta pelo ambiente que já aplica o prefixo Proton do Skyrim.
   Não execute o launcher em outro prefixo Wine.

Depois de carregar um save, pressione `F2` para abrir a interface. `Esc` ou `F2`
devolvem o mouse e o teclado ao jogo.

## Interface nativa

O CEF/Chromium usado pela interface original dispara exceções `0x80000003` dentro de
`libcef.dll` sob Proton. Flags de GPU, renderização por software e message pump
alternativo foram investigados, mas não produziram um caminho estável. Sob Wine, este
fork não inicializa o CEF e usa o renderer ImGui/D3D11 que já existia no cliente.

A interface nativa oferece:

- conexão por hostname, IPv4 ou IPv6 e suporte a senha;
- navegador público com busca, filtros, favoritos e aviso de versão;
- estado de conexão e mensagens de erro mais úteis;
- lista de jogadores;
- chat global em uma janela independente;
- tema visual inspirado nos menus de Skyrim;
- persistência dos favoritos e filtros em
  `Data/SkyrimTogetherReborn/native_overlay.json`.

No Windows nativo, o caminho CEF original continua disponível. Os arquivos do CEF
ainda podem aparecer no pacote por compatibilidade com esse build, mas não são
carregados no caminho Wine/Proton.

## O que foi alterado para funcionar no Proton

### Inicialização do jogo

O loader manual original mapeava o `SkyrimSE.exe` dentro do próprio launcher. No Wine,
as tabelas de unwind dessa imagem não ficam visíveis ao `RtlVirtualUnwind2`, causando
crashes durante exceções normais do jogo. O launcher agora detecta Wine e usa:

```text
SkyrimTogether.exe
  -> CreateProcess(SUSPENDED)
  -> restaura a imagem protegida pelo Steam CEG
  -> injeta STClientPayload.dll
  -> espera a inicialização do cliente
  -> retoma a thread principal do SkyrimSE.exe
```

O payload aceita o processo externo, aplica os patches necessários à imagem do jogo
e usa relay thunks próximos ao código-alvo para hooks relativos que não alcançam o
payload diretamente.

### Interface e input

Toda chamada de CEF fica protegida quando o runtime não foi inicializado. A UI ImGui
reutiliza o hook D3D11 do cliente e o roteamento DirectInput foi ajustado para que
`F2` capture o input somente enquanto a interface estiver visível e sempre o devolva
ao jogo ao fechar.

### Rede

O protocolo de rede foi separado da identidade do build. O fork anuncia
`PROTOCOL_VERSION = v1.8.0`, enquanto o commit continua aparecendo nos logs para
diagnóstico. Assim, as mudanças locais não tornam o cliente artificialmente
incompatível com servidores vanilla `v1.8.0`.

O aviso `non_default_install`, comum quando há Creations ou conteúdo adicional, é
informativo e não bloqueia a tentativa de conexão. Mods ainda precisam ser
compatíveis entre os jogadores e com as regras do servidor.

## Logs e diagnóstico

Os arquivos mais úteis são:

- `logs/SkyrimTogether.log`, relativo ao diretório de trabalho do launcher
  (normalmente `Data/SkyrimTogetherReborn`) — detecção do Wine, estratégia de boot,
  CEG e injeção;
- `Skyrim Special Edition/logs/tp_client.log` — inicialização do cliente, protocolo,
  autenticação, mods carregados e desconexões;
- `Data/SkyrimTogetherReborn/st_client_payload.log` — etapas iniciais do payload;
- `Data/SkyrimTogetherReborn/st_beginmain_diag.log` — marcadores da entrada do cliente.

O launcher também aceita `--verbose`, `--debug` e `--dump-config`. Em falhas de rede,
procure no `tp_client.log` pelas linhas `connecting`, `Transport connected`,
`authenticating` e `disconnected`.

Erros comuns:

- `network_timeout`: servidor offline, porta UDP bloqueada ou rota indisponível;
- `cannot_resolve_address`: hostname inválido ou falha de DNS no prefixo;
- rejeição durante autenticação: versão, senha ou política de mods do servidor;
- UI não aparece, mas o input é capturado: anexe `tp_client.log` e confirme que o
  artefato instalado corresponde ao commit mais recente.

## Limitações conhecidas

- A UI ImGui cobre conexão, servidores públicos, jogadores e chat, mas ainda não
  replica todos os recursos sociais da UI CEF original.
- Creations e mods de gameplay podem causar divergências mesmo quando a conexão é
  aceita; use uma lista compatível entre os jogadores.
- A lista pública pode conter anúncios antigos. Aparecer na lista não garante que a
  porta UDP do servidor esteja acessível.
- O caminho foi validado com Skyrim SE `1.6.1170`; outras versões precisam de teste.
- Proton, drivers e gerenciadores de mods variam entre distribuições, portanto o fork
  ainda deve ser tratado como experimental.

## Build e desenvolvimento

Cada push na branch `linux-port`, ou disparo manual do workflow
`linux-port-playable.yml`, compila os binários Windows em modo release e gera três
artefatos:

- `SkyrimTogether-linux-port (...)` — pacote jogável para o `Data`;
- `Debug Symbols (...)` — símbolos PDB;
- `linux-probe (...)` — ferramenta histórica de diagnóstico do loader.

Detalhes técnicos adicionais estão em:

- [estratégia e estado do porte](docs/linux.md);
- [decisão sobre o CEF](docs/cef-proton.md);
- [arquitetura do launcher e client](docs/architecture.md);
- [guia de build upstream](https://wiki.tiltedphoques.com/tilted-online/technical-documentation/build-guide).

Antes de contribuir, consulte [CODE_GUIDELINES.md](CODE_GUIDELINES.md) e execute
`clang-format` nos arquivos C++ alterados.

## Licença

Tilted Online é software livre sob a
[GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.html). Consulte
[LICENSE](LICENSE).
