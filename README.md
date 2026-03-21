# RP2040 Bare-Metal Bring-Up Lab

> **Fase 0 do Projeto P1 — validação arquitetural e física em baixo nível**

Este repositório documenta a validação de um ambiente **bare-metal no RP2040**, desenvolvido sem dependência do **Pico SDK**, com controle explícito de boot, memória, clocks e periféricos.

O objetivo desta fase foi comprovar, em bancada, que o microcontrolador podia ser inicializado e controlado de forma **previsível, verificável e independente de bibliotecas de alto nível**, antes da evolução para o projeto principal de extração via **SWD (Serial Wire Debug)**.

## Resumo executivo

Nesta fase foram validados, com implementação própria:

* cadeia de boot **BootROM → boot2 → aplicação**
* **linker script** com organização explícita de FLASH e RAM
* distinção prática entre **VMA** e **LMA**
* **crt0** mínimo em Assembly
* configuração manual da **clock tree**
* acesso direto a **UART, GPIO e PADS** via MMIO
* geração de artefatos **ELF, BIN e UF2**
* operação da UART com evidência funcional e elétrica

> **Nota:** o projeto principal evoluiu para repositório próprio: **[swd-forensic-extractor](https://github.com/StheffannyNAlves/swd-forensic-extractor)**.

---

## Sumário

* [Visão geral](#visão-geral)
* [Estrutura do projeto](#estrutura-do-projeto)
* [Objetivo técnico](#objetivo-técnico)
* [Arquitetura validada](#arquitetura-validada)
* [Funcionalidades implementadas](#funcionalidades-implementadas)
* [Como compilar e testar](#como-compilar-e-testar)
* [Metodologia de validação](#metodologia-de-validação)
* [Evidências experimentais](#evidências-experimentais)
* [Limitações identificadas](#limitações-identificadas)
* [Transição de arquitetura](#transição-de-arquitetura)
* [Estado do repositório](#estado-do-repositório)

---

## Visão geral

Este repositório representa a etapa de *bring-up bare-metal* do RP2040 usada como base técnica para o Projeto P1.

O foco não foi construir um firmware final nem maximizar produtividade imediata, mas validar:

* inicialização sem SDK
* controle explícito da imagem em memória
* configuração manual de clocks
* acesso direto a periféricos
* comportamento lógico e elétrico da UART em testes reais

Em outras palavras: antes de abstrair, foi necessário provar que o hardware obedecia diretamente aos registradores.

---

## Estrutura do projeto

| Diretório            | Descrição                                                                             |
| -------------------- | ------------------------------------------------------------------------------------- |
| `src/`               | Código-fonte em C e Assembly. Contém `start.s`, `main.c` e `boot2_final.S`.           |
| `linker/`            | Script de linkedição `memmap.ld`, com mapa de memória, seções e símbolos do runtime.  |
| `tools/`             | Utilitários auxiliares, incluindo `boot2.bin`, conversão para UF2 e scripts de apoio. |
| `docs/`              | Evidências experimentais, como capturas de terminal, GIFs e formas de onda.           |
| `.github/workflows/` | Pipeline de build automatizado com GitHub Actions.                                    |

---

## Objetivo técnico

A pergunta central desta fase foi:

**É possível controlar o RP2040 de forma previsível, verificável e independente de bibliotecas de alto nível?**

Para responder isso, o projeto buscou validar:

* comportamento real da cadeia de boot
* inicialização de um runtime mínimo customizado
* coerência entre layout lógico e físico da imagem
* acesso direto aos registradores do chip
* operação funcional e estabilidade elétrica da UART

### Critérios de validação atingidos

* seção `.boot2` posicionada em `0x10000000`, com **256 bytes**
* `boot2` injetado via `.incbin`, sem geração dinâmica no build
* `.text` posicionada em FLASH, `.data` carregada de FLASH e executada em RAM
* `crt0` com pilha, cópia de `.data`, limpeza de `.bss` e salto para `main`
* periféricos acessados exclusivamente por **MMIO**

---

## Arquitetura validada

### Fluxo de boot

O fluxo implementado e validado foi:

`BootROM -> boot2 -> Reset Handler -> runtime mínimo -> main()`

### Modelo de memória

A imagem foi organizada explicitamente entre FLASH e RAM:

* **FLASH**: armazenamento do firmware
* **RAM**: dados mutáveis e pilha
* **`.text`**: executada em FLASH
* **`.data`**: carregada da FLASH e copiada para RAM
* **`.bss`**: zerada manualmente no startup

### Mapa lógico resumido

```text
0x10000000  .boot2
0x10000100  .text
RAM         .data / .bss / stack
```

Essa organização foi usada para validar, na prática, a diferença entre:

* **VMA**: endereço de execução
* **LMA**: endereço de carregamento na imagem

---

## Funcionalidades implementadas

### 1. Boot e runtime manual

* vetor de interrupções definido manualmente
* `Reset_Handler` em Assembly
* configuração da pilha principal
* cópia de `.data` para RAM
* limpeza de `.bss`
* salto controlado para `main`

### 2. Clock tree manual

* habilitação do XOSC externo de 12 MHz
* espera pela estabilização do oscilador
* seleção manual de `clk_ref`
* roteamento explícito de `clk_sys`
* validação indireta pela operação correta da UART

### 3. GPIO via SIO

* configuração de direção dos pinos
* escrita por registradores atômicos
* controle lógico sem camadas intermediárias

### 4. UART bare-metal

* liberação de reset dos blocos UART e IO
* configuração de `FUNCSEL`
* definição de baud rate com `IBRD` e `FBRD`
* formato serial `115200 8N1`
* TX e RX por acesso direto às FIFOs

### 5. PADS e integridade elétrica

* análise do estado padrão dos PADS
* configuração explícita do pino RX
* ativação de *pull-up* interno
* ativação de *Schmitt trigger*
* correção da linha RX flutuante em repouso

---

## Como compilar e testar

### Requisitos

* `arm-none-eabi-gcc`
* `cmake`
* `ninja`
* `python3`

### Build

```bash
cmake -S . -B build -G Ninja
cmake --build build --verbose
```

### Artefatos esperados

```text
build/
 ├── firmware.elf
 ├── firmware.bin
 └── firmware.uf2
```

### Conversão para UF2

```bash
python3 tools/uf2conv.py build/firmware.bin -o build/firmware.uf2
```

### Teste serial

```bash
picocom -b 115200 /dev/ttyUSB0
```

---

## Metodologia de validação

A validação foi conduzida de forma incremental, isolando subsistemas sempre que necessário.

### Estratégia

1. inspeção estática da imagem com ferramentas binárias
2. validação do layout de memória
3. teste de transmissão UART
4. teste de recepção UART
5. observação da camada física com terminal serial e analisador lógico

### Ferramentas utilizadas

* `readelf`
* `objdump`
* `picocom`
* ponte USB-Serial externa
* analisador lógico Hantek 6022BL

> Durante os testes, partes do firmware foram temporariamente isoladas para validar individualmente cada subsistema antes da integração.

---

## Evidências experimentais

### Evidência 1 — comunicação serial reativa

O RP2040 executando firmware *bare-metal* em modo de eco foi conectado a um terminal serial externo.

Os testes mostraram:

* envio correto de dados ao terminal
* ausência de corrupção serial em `115200 8N1`
* recepção e eco por leitura direta da FIFO de RX

![Validação da comunicação serial reativa](./docs/uart-echo.gif)

---

### Evidência 2 — integridade de sinal em nível de fio

Foi realizada captura direta das linhas TX e RX com analisador lógico, sem *loopback* físico e sem *drive* externo adicional na linha RX.

A captura mostrou:

* presença de *start bit*, dados e *stop bit* coerentes
* TX operando conforme o comportamento esperado da UART
* RX estabilizada em nível alto após ajuste de PADS

![Captura lógica das linhas TX e RX](./docs/uart-waveform.png)

---

### Evidência 3 — validação de FIFOs por loopback físico

Para isolar variáveis externas, a linha TX foi conectada diretamente à linha RX. Nesse arranjo, o RP2040 transmitia uma sequência conhecida e validava a recepção local byte a byte.

Esse teste confirmou:

* espelhamento consistente entre TX e RX
* operação simultânea das FIFOs sem perda aparente
* integridade temporal compatível com `115200 8N1`
* recepção válida do sinal gerado pelo próprio pino de saída

![UART com loopback](./docs/testecomloopback.png)

---

## Limitações identificadas

Embora esta fase tenha validado com sucesso o domínio do RP2040 em ambiente *bare-metal*, ela também mostrou limites da UART como canal principal de exfiltração.

### Gargalo serial

* protocolo `8N1` consome 10 bits físicos por byte útil
* a `115200 bps`, o throughput efetivo fica em torno de `11.520 bytes/s`
* um dump completo de `2 MiB` leva vários minutos

Além disso, UART:

* é mais sensível a ruído
* escala mal para volumes maiores de dados
* não é a melhor escolha como canal principal para o objetivo forense final

### Conclusão

UART foi adequada como ferramenta de validação e diagnóstico, mas inadequada como canal principal de transporte da arquitetura final.

---

## Transição de arquitetura

Com base nesta fase, o projeto principal evoluiu para uma arquitetura híbrida.

### Transporte externo

Uso do **Pico SDK** apenas para a camada USB, com **TinyUSB / USB CDC**, visando:

* maior taxa de transferência
* comunicação mais estável com o host
* integração mais simples via USB

### Núcleo de extração

O controle das linhas **SWD** permaneceu *bare-metal*, preservando:

* previsibilidade temporal
* manipulação direta de GPIO e PADS
* controle fino da rotina de leitura

### Síntese

* **USB para transportar**
* **bare-metal para controlar o alvo**

---

## Estado do repositório

Este repositório deve ser entendido como:

* laboratório de validação técnica
* baseline de *bring-up bare-metal* no RP2040
* referência para a evolução do Projeto P1

### Status

* **escopo da Fase 0 concluído**
* **manutenção mínima**
* **sem objetivo de evolução para produto final**

### Fora de escopo

* transporte USB final
* implementação completa da sonda SWD
* pipeline de extração forense definitiva

---
