# CEF sob Proton/Wine — diagnóstico e decisão técnica

> Documento de decisão da **Fase 4/5** do roadmap. Registra a causa raiz do crash
> do launcher no Proton e as opções de correção, **antes** de alterar código de
> produção. Complementa [`architecture.md`](architecture.md) e [`linux.md`](linux.md).

## Sintoma

Ao lançar o `SkyrimTogether.exe` (launcher) sob Proton/Wine, uma janela aparece por
uma fração de segundo e desaparece; o launcher fecha. Relatado também por usuários de
Steam Deck/Proton — é um problema conhecido e não-trivial da comunidade.

## Evidência (crash dump do próprio STR / Sentry)

```
level: fatal
Top of stack: libcef.dll.pdb
Módulos envolvidos: chrome_elf.dll, D3DCompiler_47.dll, libcef.dll
```

## Causa raiz

O crash é **dentro do `libcef.dll`** (o Chromium embarcado), com **`D3DCompiler_47.dll`**
no topo do stack. `D3DCompiler_47` é o compilador de shaders HLSL da DirectX. O
Chromium só o toca quando o **backend gráfico acelerado (ANGLE / GPU process)** tenta
compilar shaders para compor a página.

Ou seja: o CEF está tentando **inicializar o pipeline de GPU do Chromium** (ANGLE sobre
D3D11) e a compilação de shaders quebra sob Wine/Proton. Não é o rendering do overlay
em si — esse é feito por CPU (ver abaixo).

### Por que isto é revelador

Confirmado lendo o código do overlay:

- O render handler do STR usa **`OnPaint`** com cópia por CPU para uma textura D3D11
  dinâmica — [`OverlayRenderHandlerD3D11.cpp`](../Libraries/TiltedUI/Code/ui/src/OverlayRenderHandlerD3D11.cpp)
  (`D3D11_MAP_WRITE_DISCARD` + `memcpy` do buffer do CEF). **Não** usa
  `OnAcceleratedPaint` nem shared textures de GPU. O caminho de exibição do overlay,
  portanto, **não precisa** do GPU process do Chromium.
- Ainda assim, na inicialização o CEF liga o GPU process/ANGLE por padrão, e é aí que
  o D3DCompiler é acionado e crasha.

### O que falta no código atual

Em [`OverlayApp.cpp`](../Libraries/TiltedUI/Code/ui/src/OverlayApp.cpp):

- `CefSettings`: define `no_sandbox = true`, `multi_threaded_message_loop = true`,
  `windowless_rendering_enabled = true`. **Não desabilita a GPU.**
- `OnBeforeCommandLineProcessing` (linha ~154) adiciona apenas
  `allow-file-access-from-files` e `allow-universal-access-from-files`.
  **Não passa `--disable-gpu` nem `--disable-gpu-compositing`.**

Versão do CEF: **`cef 141.0.11`** (Chromium 141) — pinada em
[`Libraries/TiltedUI/xmake.lua`](../Libraries/TiltedUI/xmake.lua). Versão recente, com
ANGLE/GPU agressivamente ligado por padrão.

## Confirmação pela documentação do CEF

- Em modo OSR (off-screen rendering), a prática recomendada é rodar com
  `--disable-gpu --disable-gpu-compositing`. O próprio `cefclient` de exemplo passa
  esses flags em modo OSR.
- Com esses flags, `CefRenderHandler::OnPaint` é chamado **direto do compositor**, sem
  cópia extra por frame — exatamente o caminho que o STR já usa.
- Para ambientes sem GPU utilizável, pode-se forçar renderização por software com
  `--use-gl=swiftshader` (ou `--use-gl=angle`/`--use-angle=swiftshader` em builds novos),
  ao custo de performance.
- `--headless` **quebra** OSR — não usar.

Conclusão: o STR está rodando OSR **sem** os flags de desabilitar GPU, então o Chromium
tenta o caminho acelerado (ANGLE→D3D→`D3DCompiler_47`), que falha sob Wine.

## Opções de correção

### Opção A — Desabilitar a GPU do Chromium (recomendada para primeiro teste)

Adicionar em `OnBeforeCommandLineProcessing`:

```cpp
aCommandLine->AppendSwitch("disable-gpu");
aCommandLine->AppendSwitch("disable-gpu-compositing");
// se ainda faltar rasterizador utilizável sob Wine:
// aCommandLine->AppendSwitchWithValue("use-gl", "swiftshader");
```

- **Prós:** mudança mínima e cirúrgica; alinhada ao modo OSR já usado; mantém a UI web
  atual; provavelmente resolve o crash do `D3DCompiler`. Não afeta Windows de forma
  perceptível (OSR por CPU continua igual).
- **Contras:** overlay 100% por CPU (o STR já é assim, então impacto baixo); se o Wine
  não tiver nem o rasterizador de software, precisa do `swiftshader`.
- **Risco:** baixo. **É o primeiro experimento a fazer.**

### Opção B — Forçar renderização por software (SwiftShader)

Como A, porém garantindo `--use-gl=swiftshader` (ou o equivalente da versão do CEF).

- **Prós:** máxima compatibilidade; independe de qualquer aceleração no Wine.
- **Contras:** mais lento; depende de a build do CEF empacotar o `swiftshader`
  (verificar os artefatos do pacote `cef 141.0.11`).
- **Risco:** baixo–médio (empacotamento).

### Opção C — Single-process

Passar `single-process` para evitar o spawn de `tp_process` e a IPC do Chromium.

- **Prós:** elimina a IPC multiprocesso (outro ponto frágil sob Wine).
- **Contras:** o CEF **não** suporta oficialmente single-process com OSR/`multi_threaded_message_loop`;
  pode introduzir instabilidade própria. Não ataca a causa raiz (que é a GPU, não a IPC).
- **Risco:** alto. Só considerar se A/B não resolverem e o problema real for IPC.

### Opção D — Remover o CEF (Fase 4 do roadmap — ImGui)

Reimplementar a UI essencial (chat, conexão, lista de players/party) em **ImGui**, que
o projeto já usa no `DebugService`, eliminando o Chromium.

- **Prós:** solução definitiva e robusta no Linux; remove um enorme peso de dependência;
  é o objetivo declarado da Fase 4.
- **Contras:** muito trabalho; reescreve toda a camada de UI (`skyrim_ui` em TS) que hoje
  vive dentro do CEF.
- **Risco:** alto esforço, baixo risco técnico. Trabalho de médio/longo prazo.

## Recomendação

1. **Opção A primeiro** (`disable-gpu` + `disable-gpu-compositing`). É a hipótese mais
   provável de resolver o crash exato do dump, com risco mínimo e sem afetar o Windows.
2. Se ainda crashar por falta de rasterizador, **Opção B** (`swiftshader`).
3. Manter **Opção D** como norte de longo prazo (Fase 4), independente do resultado acima
   — mesmo que A/B funcionem, remover o CEF continua valendo pela robustez no Linux.

Todas as mudanças de flag devem ser guardadas para **não alterar o comportamento no
Windows** quando desnecessário (ex.: aplicar sob `#if defined(__WINE__)`/detecção de
Proton, ou aceitar que desabilitar GPU em OSR é inócuo no Windows e aplicar sempre —
a decidir com base em teste).

## Como validar (checklist de teste no Proton)

Para o próximo ciclo, ao aplicar a Opção A:

1. Confirmar a versão/artefatos do pacote `cef 141.0.11` (há `swiftshader`? `libcef.dll`,
   `chrome_elf.dll`, `locales/`, `*.pak` presentes ao lado do launcher?).
2. Rodar sob Proton com log verboso:
   - CEF já escreve `logs/cef_debug.log` (`log_severity = VERBOSE`, ver `OverlayApp.cpp`).
   - `PROTON_LOG=1` (gera `steam-<appid>.log` no home) e/ou `WINEDEBUG=+loaddll` para ver
     quais DLLs carregam/falham.
3. Verificar no `cef_debug.log` se o GPU process ainda é iniciado e se o
   `D3DCompiler_47` some do caminho de crash.
4. Confirmar se o overlay (menu/chat) renderiza in-game.

> Nota: nada disso foi testado ainda nesta máquina; o launcher exige toolchain Windows
> para compilar e um ambiente Proton com Skyrim SE para rodar. Este documento é o plano.
```
