# CEF sob Proton/Wine — diagnóstico e decisão implementada

> Registro técnico do bloqueio que levou à UI ImGui. A conclusão atual é: **CEF não é
> inicializado sob Wine/Proton**. No Windows nativo, o caminho upstream é preservado.

## Sintoma observado

Depois que o loader e o payload passaram a iniciar o jogo, o processo ainda encerrava
com exceções como:

```text
0x80000003 em libcef.dll
VectoredExceptionHandler: crash occurred
```

Os endereços mudavam entre execuções, mas permaneciam dentro de `libcef.dll`. Em
algumas tentativas o CEF chegava mais longe antes do `int3`; em outras, a exceção
acontecia durante `CefInitialize` ou criação do browser.

## Hipótese inicial

O overlay usa `CefRenderHandler::OnPaint` e copia por CPU o buffer do CEF para uma
textura D3D11. Portanto, ele não precisa de `OnAcceleratedPaint`. A primeira hipótese
foi que o Chromium inicializava ANGLE/GPU desnecessariamente sob Wine e falhava no
pipeline que envolve `D3DCompiler_47.dll`.

Foram avaliados ou implementados experimentalmente:

- `--disable-gpu` e `--disable-gpu-compositing`;
- renderização por software/SwiftShader;
- desligamento de recursos de rede do Chromium;
- `single-process`;
- `external_message_pump`;
- resolução explícita dos diretórios de recursos do CEF;
- logs e marcadores antes/depois de `CefInitialize` e `CreateBrowser`;
- carregamento isolado de `libcef.dll` e handlers de diagnóstico.

Essas tentativas foram importantes para separar o crash do loader, do payload e do
loop do jogo. Nenhuma tornou o CEF confiável no ambiente Proton testado.

## Decisão

O projeto já possuía renderer ImGui sobre D3D11 para ferramentas internas. Reusar essa
infraestrutura é menor e mais previsível do que manter um runtime Chromium completo
dentro do processo do Skyrim sob Wine.

Quando Wine é detectado:

1. `OverlayService` não cria o runtime CEF;
2. eventos que normalmente chamariam CEF verificam se ele foi inicializado;
3. `ImGuiOverlayService` fornece conexão, servidores públicos, jogadores e chat;
4. `InputService` direciona teclado/mouse para ImGui apenas enquanto a UI está aberta;
5. fechar com `F2` ou `Esc` devolve o input ao jogo.

No Windows nativo, a implementação CEF original continua sendo construída e usada.
Por isso, dependências e recursos CEF ainda podem estar presentes no artefato geral,
embora o caminho Proton não os carregue.

## Resultado

Com o CEF fora do caminho Wine:

- novo jogo e saves antigos carregaram sem o `int3` de `libcef.dll`;
- o overlay abriu dentro do jogo e liberou corretamente o input;
- a lista pública e a autenticação em servidor vanilla `v1.8.0` funcionaram;
- chat e lista de jogadores passaram a usar diretamente os serviços do client.

Esse resultado confirma a decisão para o fork, mas não prova que o CEF seja impossível
de executar em toda versão de Wine. Ele apenas não é mais uma dependência funcional do
cliente Linux/Proton.

## Escopo ainda não migrado

A UI ImGui atual cobre o caminho essencial de multiplayer. Recursos sociais mais
complexos da UI TypeScript/CEF, principalmente party/grupo, ainda precisam ser
reimplementados para paridade completa.

O código em `Code/skyrim_ui`, `Code/tp_process` e `Libraries/TiltedUI` permanece
necessário para o build Windows nativo e como referência de comportamento.

## Diagnóstico futuro

Se um crash sob Proton voltar a apontar para CEF, primeiro confirme no log que a build
realmente detectou Wine e exibiu a decisão de pular o overlay CEF. Uma carga de
`libcef.dll` nesse caminho indica regressão ou DLL externa; não deve ser tratada apenas
com mais flags do Chromium.

Consulte também:

- [estado do porte](linux.md);
- [arquitetura geral](architecture.md);
- [README de instalação](../README.md).
