# RP2040 Bare-Metal Bring-Up Lab

> **Fase 0 do Projeto P1 — validação arquitetural e física em baixo nível**

Este repositório documenta um laboratório isolado de pesquisa, validação e *bring-up bare-metal* do **RP2040**, desenvolvido como base técnica para o Projeto P1. Toda a pilha mínima de inicialização foi construída manualmente, **sem dependência do Pico SDK**, com foco em controle explícito de boot, memória, clocks e periféricos.

O propósito desta fase foi comprovar, na prática, domínio determinístico sobre o hardware antes da evolução para a ferramenta final de extração via **SWD (Serial Wire Debug)**. Em vez de abstrações prontas, a proposta aqui foi entender e validar o comportamento do microcontrolador diretamente no nível de registradores e mapa físico de memória.

Os principais eixos de validação desta fase foram:

- cadeia de boot do silício (**BootROM → boot2 → aplicação**)
- organização física de memória (**FLASH/RAM, VMA vs LMA**)
- runtime mínimo (**crt0 customizado em Assembly**)
- configuração manual de osciladores e *clock tree*
- acesso direto a periféricos via MMIO (**UART, GPIO e PADS**)
- pipeline de build raiz (**linker script + ELF + BIN + UF2**)

> **Nota:** o desenvolvimento ativo do projeto principal, a **Sonda Forense de Extração de Memória via SWD**, encontra-se em repositório próprio: **[swd-forensic-extractor](https://github.com/StheffannyNAlves/swd-forensic-extractor)**.

---

## Sumário

- [Visão geral](#visão-geral)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Objetivo técnico da Fase 0](#objetivo-técnico-da-fase-0)
- [Arquitetura validada](#arquitetura-validada)
- [Funcionalidades implementadas](#funcionalidades-implementadas)
- [Pipeline de build](#pipeline-de-build)
- [Metodologia de teste e evidências](#metodologia-de-teste-e-evidências)
- [Resultados técnicos](#resultados-técnicos)
- [Limitações identificadas](#limitações-identificadas)
- [Transição de arquitetura](#transição-de-arquitetura)
- [Conclusão](#conclusão)

---

## Visão geral

Este repositório representa a etapa de validação fundamental do ambiente *bare-metal* no RP2040. O objetivo não foi construir uma aplicação final nem maximizar produtividade imediata, mas estabelecer uma base tecnicamente verificável para o restante do projeto.

Na prática, isso significou:

- partir de um ambiente mínimo, sem SDK
- definir manualmente o processo de inicialização
- controlar o posicionamento de seções em memória
- configurar clocks e periféricos diretamente por registradores
- validar eletricamente a operação da UART
- comprovar a viabilidade de uma abordagem de baixo nível antes de escalar para o caso de uso forense

Essa fase funcionou como uma prova de domínio sobre o microcontrolador. O foco foi menos “fazer aparecer algo no terminal” e mais “entender exatamente por que aquilo apareceu, de onde veio e qual registrador tornou isso possível”. Sim, o caminho mais longo. Justamente por isso, o que ficou validado aqui vale mais do que meia dúzia de exemplos com HAL envernizada.

---

## Estrutura do projeto

| Diretório | Descrição |
| --- | --- |
| `src/` | Código-fonte em C e Assembly. Contém `start.s` (runtime e Reset Handler), `main.c` (drivers e lógica de teste por MMIO) e `boot2_final.S` (injeção do estágio `boot2` com `.incbin`). |
| `linker/` | Script de linkedição `memmap.ld`, responsável pelo mapa de memória, símbolos do runtime, posicionamento das seções e definição explícita de VMA/LMA. |
| `tools/` | Artefatos e utilitários auxiliares, como `boot2.bin` validado, conversão para UF2 e scripts de apoio ao fluxo de testes. |

---

## Objetivo técnico da Fase 0

A Fase 0 foi concebida para responder a uma pergunta objetiva: **é possível controlar o RP2040 de forma previsível, verificável e independente de bibliotecas de alto nível?**

Para isso, esta etapa buscou validar:

- o comportamento real da cadeia de boot
- a inicialização de um runtime mínimo customizado
- a consistência entre layout lógico e layout físico da imagem
- o acesso direto a registradores de controle
- a integridade elétrica e lógica da UART em modo *bare-metal*

A filosofia do repositório é deliberadamente conservadora: antes de abstrair, é preciso validar. Antes de otimizar, é preciso medir. Antes de confiar numa stack pronta, é preciso demonstrar que o hardware obedece sem ela.

### Critérios de validação arquitetural atingidos

- **Isolamento de boot:** a seção `.boot2` foi posicionada rigorosamente em `0x10000000`, com exatamente **256 bytes**, como exigido pelo RP2040.
- **Injeção determinística de `boot2`:** o arquivo `boot2_final.S` utiliza `.incbin` para inserir um binário previamente preparado, evitando dependência de geração dinâmica de CRC32 no processo de compilação.
- **Mapeamento explícito de memória:** a seção `.text` foi colocada em `0x10000100`, enquanto `.data` foi definida para execução em RAM e carregamento inicial em FLASH, com uso explícito de **LMA** e **VMA**.
- **Runtime mínimo funcional:** o `crt0` implementa configuração do *stack pointer*, cópia da `.data`, limpeza da `.bss` e salto controlado para `main`.
- **Acesso direto a periféricos:** UART e GPIO foram validados exclusivamente por MMIO, sem HAL, sem SDK e sem suporte de bibliotecas padrão.

---

## Arquitetura validada

A seguir, um resumo do fluxo arquitetural implementado nesta fase:

### 1. Boot físico do RP2040

O fluxo validado segue a cadeia real de inicialização do chip:

BootROM (silício) → boot2 em FLASH → Reset Handler → runtime mínimo → main()

O papel de cada estágio é o seguinte:

- **BootROM:** estágio residente no silício, responsável por localizar e executar o segundo estágio de boot.
- **boot2:** pequeno código posicionado no início da FLASH, necessário para preparar a interface XIP.
- **Reset Handler:** ponto de entrada da aplicação.
- **runtime mínimo:** configuração inicial do ambiente de execução em RAM.
- **main():** início da lógica funcional da aplicação.

### 2. Modelo de memória

A estrutura de memória foi organizada para refletir explicitamente as restrições físicas do RP2040:

- **FLASH:** armazenamento da imagem do firmware.
- **RAM:** execução de dados mutáveis e pilha.
- **`.text`:** executada a partir da FLASH.
- **`.data`:** carregada da FLASH e copiada para RAM no *startup*.
- **`.bss`:** zerada manualmente no *startup*.

Esse arranjo foi importante não só para fazer a imagem rodar, mas para confirmar entendimento real sobre a diferença entre:

- **VMA (Virtual Memory Address):** endereço de execução da seção.
- **LMA (Load Memory Address):** endereço de carregamento da seção na imagem.

Em projeto embarcado, ignorar isso costuma produzir bugs bonitos e silenciosos. A pior espécie.

---

## Funcionalidades implementadas

### 1. Boot e runtime manual

A inicialização do firmware foi construída sem dependência de runtime externo, com controle explícito desde o vetor de interrupções até a transferência de execução para `main`.

Esse bloco inclui:

- vetor de interrupções com ponto de entrada definido manualmente
- `Reset_Handler` implementado em Assembly
- configuração da pilha principal
- cópia manual da seção `.data` da FLASH para a RAM
- limpeza da seção `.bss`
- salto controlado para `main`

Com isso, foi possível validar a transição entre imagem em FLASH e execução em RAM, eliminar dependência de *startup code* gerado por terceiros e garantir previsibilidade do estado inicial do processador.

### 2. Clock tree manual

A configuração de clocks foi feita por acesso direto aos registradores dos blocos `XOSC` e `CLOCKS`, sem apoio de rotinas do SDK.

Foram implementados:

- habilitação do XOSC externo de 12 MHz
- espera ativa pela estabilização do oscilador
- seleção manual da fonte de `clk_ref`
- roteamento explícito de `clk_sys`
- validação prática da frequência resultante por meio da operação correta da UART

Essa etapa confirmou domínio sobre a origem do clock do sistema, consistência temporal para periféricos dependentes de frequência e independência da inicialização automática oferecida por bibliotecas externas.

### 3. GPIO via SIO

O controle de GPIO foi realizado diretamente no bloco `SIO`, usando registradores atômicos de *set*, *clear* e *xor* para manipulação do estado lógico dos pinos.

A implementação cobriu:

- configuração da direção dos pinos
- escrita em saídas por registradores atômicos
- controle explícito do nível lógico sem camada intermediária

O resultado foi a validação de acesso direto a I/O digital, previsibilidade de operação em ciclo único e compreensão prática da separação entre mux de função e bloco de saída digital.

### 4. UART bare-metal

A UART foi configurada inteiramente por MMIO, incluindo liberação de reset, mux de pinos, divisores de baud rate e acesso direto às FIFOs de transmissão e recepção.

As etapas principais foram:

- liberação dos blocos UART e IO de *reset*
- configuração de função alternativa dos pinos por `FUNCSEL`
- definição de baud rate por `IBRD` e `FBRD`
- configuração do formato `115200 8N1`
- transmissão e recepção por acesso direto à FIFO

Essa validação confirmou o funcionamento do periférico sem HAL, a capacidade de operar TX e RX diretamente em nível de registradores e a coerência entre clock configurado e temporização serial observada em teste.

### 5. Auditoria de PADS e integridade elétrica

Além da configuração lógica dos periféricos, foi necessário validar o comportamento físico dos pinos, com foco especial na linha RX da UART.

A auditoria incluiu:

- análise do estado padrão dos PADS após *reset*
- configuração explícita do comportamento elétrico do pino RX
- ativação de *pull-up* interno
- ativação de *Schmitt trigger*
- eliminação da condição de linha flutuante em repouso

Essa etapa demonstrou domínio sobre a camada elétrica do microcontrolador, correção de comportamento anômalo por configuração explícita de PADS e aumento de robustez da recepção contra ruído e indefinição lógica.

Mais importante: não bastava observar que “a UART às vezes funcionava”. Era necessário identificar por que falhava, em que condição elétrica isso acontecia e qual registrador corrigia o problema. Sem isso, a análise viraria superstição com instrumentação cara.

---

## Pipeline de build

O fluxo de build foi montado para manter o controle total sobre a imagem final.

### Componentes principais

- compilação cruzada com `arm-none-eabi-gcc`
- linkedição com *linker script* customizado
- geração de ELF com layout previsível
- inspeção com `readelf` e `objdump`
- conversão da imagem para formatos graváveis (`BIN`/`UF2`)

### Objetivos do pipeline

- garantir posicionamento correto das seções
- evitar dependências implícitas de bibliotecas externas
- permitir auditoria do binário em cada etapa
- produzir artefatos determinísticos para validação

---

## Metodologia de teste e evidências

A validação foi conduzida incrementalmente, com foco em isolamento de subsistemas e evidência observável.

### Estratégia de teste

1. inspeção estática da imagem com ferramentas de binário
2. validação do layout de memória
3. testes de transmissão UART
4. testes de recepção UART
5. observação da camada física com terminal serial e analisador lógico

### Ferramentas utilizadas

- `readelf`
- `objdump`
- Terminal serial (`picocom`)
- Ponte USB-Serial externa baseada em Cypress FX2
- Analisador lógico Hantek 6022BL para inspeção de TX/RX

> **Observação:** durante a fase de testes, partes do firmware foram temporariamente isoladas ou comentadas para permitir validação individual de cada subsistema. O objetivo desta etapa foi comprovação técnica em baixo nível, não integração final de todas as funcionalidades em uma única versão estática do código.

---

## Resultados técnicos

### Evidência 1 — comunicação reativa e integridade lógica

O RP2040 executando firmware *bare-metal* em modo de eco foi conectado a um terminal serial por meio de uma ponte USB-Serial externa. Nesse arranjo, foi possível observar a transmissão, a recepção e o reenvio dos caracteres ponta a ponta, sem apoio de HAL ou SDK.

Os testes mostraram:

- envio correto de dados ao terminal
- ausência de corrupção serial em `115200 8N1`
- recepção e eco de caracteres por leitura direta da FIFO de RX

Esses resultados indicam que o *clock tree* estava compatível com a taxa serial configurada, que o cálculo dos divisores da UART estava coerente e que o periférico operava corretamente em transmissão e recepção por MMIO direto.

A imagem abaixo registra a validação funcional da comunicação serial em modo reativo, evidenciando o comportamento esperado do firmware durante o teste.

![Validação da comunicação serial reativa](./docs/uart-echo.gif)

### Evidência 2 — integridade de sinal em nível de fio

Além da validação lógica no terminal, foi realizada captura direta das linhas TX e RX com analisador lógico, sem *loopback* físico e sem *drive* externo adicional na linha RX.

A inspeção dos sinais revelou:

- presença de *start bit*, dados e *stop bit* com estrutura temporal coerente
- linha TX operando dentro do comportamento esperado da UART
- linha RX estabilizada em nível lógico alto após ajuste de PADS

Essa observação confirma que a temporização gerada pelo periférico UART estava correta e que a estabilidade elétrica da linha RX passou a ser garantida localmente, sem depender de condicionamento externo. Também demonstra que a configuração manual dos PADS resolveu a instabilidade observada no estado padrão do pino.

A captura abaixo documenta a integridade temporal do protocolo e a estabilização elétrica da linha RX após a configuração explícita dos PADS.

![Captura lógica das linhas TX e RX](./docs/uart-waveform.png)

### Evidência 3 — validação de FIFOs por loopback físico

Para isolar o microcontrolador de variáveis externas, como cabos, conectores e conversores USB-Serial, a linha de TX foi conectada diretamente à linha de RX (`GPIO 0` ao `GPIO 13`, ou conforme o mapeamento físico utilizado no teste). Nesse arranjo, o próprio RP2040 transmitia uma sequência conhecida (`"Sthe\r\n"`) e validava internamente a recepção byte a byte, acionando o LED apenas quando a carga útil retornava corretamente.

![UART com loopback](docs/testecomloopback.png)

Esse teste confirmou:

- espelhamento consistente entre transmissão e recepção no mesmo enlace físico
- operação simultânea das FIFOs de TX e RX sem perda aparente de dados
- integridade temporal compatível com a configuração de `115200 8N1`
- recepção válida do sinal gerado pelo próprio pino de saída

Na prática, o loopback físico mostrou que o periférico UART conseguia transmitir, receber e validar a própria carga útil sem depender de ponte serial externa ou condicionamento adicional de sinal. Isso reforça que tanto a configuração lógica da UART quanto o comportamento elétrico dos pinos estavam coerentes durante a execução do teste.

Esse experimento também foi útil para separar problemas de firmware e hardware interno de problemas externos de bancada. Se o dado sai, retorna e é reconhecido corretamente pelo próprio microcontrolador, então a base do enlace local está íntegra; o resto deixa de ser culpa “mística” do chip e passa a ser suspeita legítima sobre o ambiente externo.

---

## Limitações identificadas

Embora esta fase tenha validado com sucesso o domínio do RP2040 em ambiente *bare-metal*, ela também revelou limitações práticas para o caso de uso final da sonda forense.

### Gargalo de exfiltração serial

O uso de UART como canal principal de transporte externo impõe um custo estrutural de desempenho:

- protocolo `8N1` consome 10 bits físicos por byte útil
- a `115200 bps`, o *throughput* efetivo fica em torno de `11.520 bytes/s`
- um *dump* completo de `2 MiB` ultrapassa facilmente 3 minutos de extração

Além da limitação de taxa, enlaces seriais físicos simples:

- são mais sensíveis a ruído
- escalam mal em volume de dados
- não representam a melhor escolha para fluxo forense de maior capacidade

#### Conclusão dessa limitação

UART foi excelente como ferramenta de validação e diagnóstico em baixo nível, mas inadequada como canal principal de exfiltração para a arquitetura final do projeto.

---

## Transição de arquitetura

A partir das conclusões desta Fase 0, o projeto principal evoluiu para uma arquitetura híbrida, separando claramente o que deve permanecer *bare-metal* daquilo que pode ser abstraído sem prejuízo técnico.

### Modelo adotado

#### 1. Transporte forense

Uso do Pico SDK exclusivamente para a camada de comunicação USB, com TinyUSB / USB CDC, a fim de obter:

- maior taxa de transferência
- comunicação assíncrona mais estável
- integração *plug-and-play* com o host
- redução de fragilidade elétrica típica da UART em cabos simples

#### 2. Núcleo de extração

O controle das linhas SWD do chip alvo permanece rigorosamente *bare-metal*, preservando:

- controle de tempo
- previsibilidade
- manipulação direta de GPIO/PADS
- capacidade de implementação de rotinas de leitura segura (*safe-read*)

### Justificativa técnica

Essa separação não representa abandono da abordagem de baixo nível. Pelo contrário: ela usa o que foi validado aqui para decidir, com critério, onde o *bare-metal* é essencial e onde abstração bem escolhida melhora o sistema.

Em outras palavras:

- **USB para transportar**
- **bare-metal para controlar o alvo**

Misturar essas duas responsabilidades no mesmo grau de rigidez seria mais dogma do que engenharia.

---

## Conclusão

Este repositório registra a fundação técnica do Projeto P1.

Ao longo desta fase, foram validados com sucesso:

- a cadeia de boot do RP2040
- o posicionamento físico e lógico da imagem em memória
- um runtime mínimo customizado
- a configuração manual da *clock tree*
- o controle direto de GPIO, UART e PADS
- a operação prática de comunicação serial *bare-metal*
- a correção explícita de problemas elétricos na linha RX

Este laboratório serviu como base de decisão arquitetural. Demonstrou quais partes do sistema exigem controle absoluto em baixo nível e quais podem ser delegadas a uma camada de suporte sem comprometer os objetivos do projeto.

**Este repositório não é o produto final.**  

---

## Estado do repositório

Este repositório deve ser entendido como:

- laboratório histórico de validação
- *baseline* técnico de *bring-up bare-metal*
- referência de baixo nível para a evolução do Projeto Sonda SWD