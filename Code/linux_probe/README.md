# linux_probe — validação da hipótese de unwind sob Proton

Ferramenta de **diagnóstico**, não parte do produto. Existe para responder a uma
única pergunta antes de qualquer refactor:

> O crash do Skyrim sob Proton acontece porque o `SkyrimSE.exe` é **auto-mapeado**
> pelo `ExeLoader` (mapeamento manual de PE), deixando as unwind tables invisíveis
> para o `RtlVirtualUnwind2` do Wine?

## A hipótese

O diagnóstico atual (ver `docs/cef-proton.md` e o histórico da branch `linux-port`)
aponta para o seguinte encadeamento:

1. Dentro de `gameMain()` o Skyrim nomeia threads via o mecanismo legado do MSVC,
   `RaiseException(0x406D1388)` = `EXCEPTION_WINE_NAME_THREAD`.
2. No Windows com debugger isso é absorvido. Sob Wine sem debugger a exceção é
   despachada de verdade.
3. Um vectored handler dentro do jogo retorna `EXCEPTION_CONTINUE_SEARCH`, então o
   `RtlVirtualUnwind2` percorre a stack e entra no código auto-mapeado (base
   `0x140000000`).
4. Lá ele não encontra unwind info utilizável — segue RIPs inválidos (`0`, `0x1000`,
   `0x19c`) — e a cascata termina em SIGFPE (exit code 136).

A causa fundamental não é a exceção `0x406D1388` em si: é que **a exception/unwind
table de um módulo auto-mapeado não é registrada do jeito que o Wine precisa**. O
próprio `ExeLoader.cpp` reconhece isso:

```cpp
// has no use - inverted function tables get used instead from Ldr;
// we have no influence on those
RtlAddFunctionTable(...);
```

`RtlAddFunctionTable` registra uma tabela *dinâmica*, que é a segunda escolha do
lookup. A primeira é a tabela do módulo carregado pelo loader
(`LdrpInvertedFunctionTable`), que nunca é populada para uma região de memória que
o loader não conhece como imagem.

Consumir a exceção `0x406D1388` num vectored handler removeria apenas **um**
gatilho. O Skyrim, o CRT e o D3D lançam SEH legítimo em outros pontos, e cada um
deles reencontraria o mesmo unwind quebrado. Por isso a hipótese a testar é a
estrutural, não o paliativo.

## O que este probe faz

Se a hipótese estiver certa, deixar o **loader do Wine** carregar o
`SkyrimSE.exe` (via `CreateProcess` normal) faz o problema desaparecer, porque o
Wine popula as estruturas de imagem ele mesmo. O probe monta exatamente esse
cenário, no menor tamanho possível:

- `probe_loader.exe` — cria o `SkyrimSE.exe` **suspenso**, injeta
  `probe_payload.dll` via `CreateRemoteThread`+`LoadLibraryW`, e dá `ResumeThread`.
  É a mesma janela de execução que o SKSE usa: a DLL roda antes do entry point do
  jogo.
- `probe_payload.dll` — **não** carrega o client do STR nem instala hooks. Só
  instala um vectored exception handler que loga toda exceção despachada (com
  código, endereço e módulo) e devolve `EXCEPTION_CONTINUE_SEARCH`, sem alterar o
  comportamento. Puro observador.

O objetivo é isolar a variável **modo de carregamento** e nada mais. Nenhum
código do STR roda dentro do jogo neste teste.

## Como interpretar o resultado

| Observação | Leitura |
| --- | --- |
| Jogo chega ao menu principal; log mostra `0x406D1388` despachada e seguida sem crash | **Hipótese confirmada.** O auto-mapeamento é a causa. O refactor para injeção externa se justifica. |
| Jogo crasha do mesmo jeito (SIGFPE / exit 136), unwind seguindo RIPs inválidos | **Hipótese refutada.** O problema não é o modo de carregamento; o refactor inteiro fica economizado. |
| Jogo crasha de forma diferente (ex.: DRM/CEG da Steam reclamando) | Resultado parcial: o unwind pode ter sido resolvido, mas há outro bloqueador. O log do handler diz qual. |

Esse terceiro caso é a ressalva conhecida: o STR já lida com DRM CEG da Steam
(`steam/SteamCeg.cpp`, `steam/SteamCrypto.h`), e não está estabelecido como o
anti-tamper se comporta com o EXE rodando como processo real em vez de mapeado à
mão. O probe é justamente o que responde isso barato.

## Como rodar (Proton, na máquina do usuário)

Os binários são PE/Windows — precisam vir do CI (não há toolchain de build local
para PE nesta máquina). Baixe o artifact e coloque os dois arquivos juntos numa
pasta qualquer dentro do prefixo.

```bash
export PROTON="$HOME/.local/share/Steam/compatibilitytools.d/GE-Proton11-1/proton"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
export STEAM_COMPAT_DATA_PATH="$HOME/.local/share/Steam/steamapps/compatdata/489830"
export STEAM_COMPAT_APP_ID=489830
export PROTON_LOG=1
export WINEDEBUG=+seh,+unwind

"$PROTON" run /caminho/para/probe_loader.exe "C:\\caminho\\para\\SkyrimSE.exe"
```

Saídas a coletar:

- `probe_loader.log` e `probe_payload.log`, criados ao lado do `probe_loader.exe`.
- `~/steam-489830.log` (do `PROTON_LOG=1`), que contém o rastro `+seh,+unwind`.

O `WINEDEBUG=+seh,+unwind` é o que torna o resultado legível: é nele que se vê se
o `RtlVirtualUnwind2` ainda segue RIPs inválidos ao entrar na base do jogo.

## Limites deliberados

Este probe **não** é um caminho de produção e não deve virar um. Ele não faz
usvfs/PathRerouting, não reserva a zona do Script Extender, não instala os stubs
de API, e não carrega o client. Um `ExternalProcessInjector` de verdade precisa de
tudo isso — o que é o escopo da Fase 2 do roadmap (IPlatform), e só se justifica
depois que este teste passar.
