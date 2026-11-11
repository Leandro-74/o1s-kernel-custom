# Protocolo de validação do undervolt

| Campo | Valor |
|---|---|
| Status | em execução |
| Mudança validada | [22 — Undervolt por módulo de kernel](alteracoes/22-undervolt-modulo-kernel.md) |
| Casos | 0, 1, 2 e 3 (ver tabela) |
| Duração total | ~17 h, em 3 a 4 dias |
| Ferramenta | harness local `o1s-lab`, fora do repositório |

Este documento define como os casos de undervolt são medidos e comparados. O
objetivo é chegar a um default que reduza consumo sem perder desempenho e sem
instabilidade — em especial em idle longo e suspend, onde o undervolt agressivo
costuma causar reboot silencioso.

## Casos

| Caso | CL0 (little) | CL1 (big) | CL2 (prime) | GPU (G3D) |
|---|---|---|---|---|
| 0 (controle) | 0 | 0 | 0 | 0 |
| 1 | 1 | 1 | 1 | 1 |
| 2 | 2 | 2 | 2 | 1 |
| 3 | 3 | 3 | 3 | 1 |

Os valores são percentuais: `2` reduz 2% da tensão do domínio. Fora de teste, o
harness mantém os quatro domínios em `0`.

## Critérios de aprovação

Um caso só é aprovado se cumprir os quatro critérios:

1. **Estabilidade** — nenhum reboot, panic, erro de ACPM/GPU ou app encerrado
   durante os cenários, inclusive depois de suspend.
2. **Pico** — desempenho de pico dentro de 2% do caso 0 (ruído entre rodadas).
3. **Sustentado** — temperatura igual ou menor e score médio igual ou maior que
   o caso 0 nos testes de stress.
4. **Consumo** — drain em idle mensuravelmente menor que o caso 0, fora do
   ruído.

Instabilidade elimina o caso, independentemente do ganho. Um caso que empata em
tudo com o caso 0 também é descartado: não vale assumir risco sem benefício.

## Condições controladas

Todas as rodadas seguem o mesmo roteiro:

- mesmo build, kernel, config e governador (`energy_step`);
- versões dos apps fixas e atualização automática desativada;
- **reboot antes de cada caso**, seguido de 10 min de acomodação com a tela
  desligada;
- bateria entre 80% e 100%, **sem carregador** durante todo o teste;
- temperatura da bateria ≤ 30 °C no início (senão aguardar esfriar);
- modo avião ligado, Wi-Fi e dados desligados;
- brilho fixo em 50%, auto-brilho desligado, quando a tela precisa ficar ligada;
- nenhum uso do aparelho durante a janela;
- registrar temperatura, carga e offsets no início e no fim.

O harness faz o pré-voo automático e recusa iniciar se bateria, carregador ou
temperatura estiverem fora da faixa.

## Cenários

### A — Idle (3 h por caso)

Tela desligada, modo avião, sem carga. É o cenário que expõe instabilidade de
suspend. Métricas: %/h, mAh/h, corrente média, tempo suspenso, temperatura.

### B — Stress CPU (20 min por caso)

CPU Throttling Test 1.3.4, **8 threads**, duração de 20 min, estilo de carga
padrão do app. O app fornece máximo, mínimo e média de GIPS; a curva mostra
onde o throttling começa. O harness coleta energia, temperatura e frequência
em paralelo, com a tela ligada em brilho fixo.

### C — Stress GPU (20 min por caso)

3DMark Wild Life Stress Test (20 loops). Métricas: estabilidade %, FPS médio,
temperatura máxima e frequência da GPU.

### D — Desempenho (~45 min por caso)

A mesma suíte manual já registrada em [benchmarks](benchmarks.md):

1. 3DMark Wild Life;
2. Geekbench 7 (CPU single/multi, GPU OpenCL e Vulkan);
3. AnTuTu v12.

Com 15 min de resfriamento entre os apps. O harness apenas marca a janela; os
apps são operados manualmente.

## Ordem e repetição

1. Executar em ordem ascendente: **0 → 1 → 2 → 3**. Se um caso reiniciar ou
   falhar, interromper e não testar os mais agressivos naquele dia.
2. Repetir **0 e 3** ao final, no formato A/B/A, para medir a deriva térmica e
   ambiental.
3. Se a repetição do caso 0 divergir mais que o ruído observado, refazer a
   comparação antes de concluir qualquer coisa.

## Execução com o harness

O harness `o1s-lab` é uma ferramenta local, instalada como módulo do KernelSU
apenas no aparelho de teste. **Não faz parte do repositório e nenhuma
funcionalidade do kernel depende dele.**

Fluxo pelo painel do KernelSU:

- **Idle:** escolher caso, cenário `Idle` e duração; tocar em *Iniciar idle*.
  O harness liga o modo avião, desliga a tela e encerra sozinho no prazo.
- **Stress CPU:** tocar em *Preparar caso e abrir app*; iniciar o teste no CPU
  Throttling Test (8 threads, 20 min); tocar em *Começar cronômetro*.
- **Stress GPU:** mesmo fluxo, com o Wild Life Stress Test.
- **Desempenho:** preparar o caso, rodar a suíte manualmente e tocar em
  *Concluir e gerar relatório*.
- **Abortar** encerra, gera relatório parcial e zera os offsets.

O mesmo pode ser feito por linha de comando:

```sh
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh status'
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh start 2 idle 180'
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh arm 2 stress-cpu'
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh go 20'
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh finish'
su -c 'sh /data/adb/modules/o1s-lab/bin/testctl.sh reset'
```

Saída em `/sdcard/o1s-lab/`:

```
power/AAAA-MM.csv                       amostras de 60 s
tests/<data>-caso<N>-<cenario>/
  resumo.md                             relatório da janela
  amostras.csv                          recorte da janela
  screenshot.png                        tela final (stress)
incidents.log                           falhas e abortos
```

## Segurança

- Pré-voo: bateria ≥ 80%, sem carregador, bateria ≤ 30 °C.
- Aborto automático se a bateria passar de 45 °C ou cair abaixo de 15%.
- Reboot com teste em andamento é registrado como instabilidade, os offsets
  voltam a zero e o caso é marcado como abortado.
- Modo avião e Wi-Fi anteriores são restaurados ao encerrar.

## Leitura do relatório

O relatório é factual: consumo, temperatura, frequência, suspensão e alertas.
Uma rodada é **inválida** se o relatório apontar carregador conectado, reboot ou
suspensão inesperada fora do cenário de idle. Nesse caso, repetir.

Comparação entre casos:

- **Idle:** usar `%/h` e `mAh/h`; conferir se o tempo suspenso foi parecido.
- **Stress CPU:** comparar GIPS médio e mínimo do app, além da temperatura
  máxima; o mínimo é o indicador mais direto de throttling.
- **Stress GPU:** comparar estabilidade % e FPS médio.
- **Desempenho:** comparar os scores com o caso 0; diferença dentro de 2% é
  ruído.

## Resultados

Preencher ao final de cada rodada:

| Caso | Idle %/h | Idle mAh/h | CPU GIPS méd/min | CPU temp máx | GPU estab. % | 3DMark | GB single/multi | AnTuTu | Veredito |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0,329 | 11,1 | — | — | — | — | — | — | controle |
| 1 | 0,329 | 8,6 | — | — | — | — | — | — | empate técnico com 0 |
| 2 | não executado | — | — | — | — | — | — | — | dispensado |
| 3 | não executado | — | — | — | — | — | — | — | adotado como default |

### Rodada 1 — caso 0 (controle)

- Janela: 2026-11-10T14:24:01 → 17:24:01; o encerramento efetivo ocorreu às
  17:26:34 porque a geração do relatório foi atrasada pelo suspend.
- Bateria 100% → 99%: 33,68 mAh, **0,329 %/h** e **11,1 mAh/h**.
- Offsets `0 0 0 0`; modo avião ligado; tela desligada em 100% do tempo; sem
  carregador e sem reboot.
- Suspensão: 2 intervalos, 10 984 s no total (maior 7 248 s). O coletor só
  registrou 2 amostras; por isso o consumo sai das leituras de bateria no
  início e no fim, e não da média das amostras.
- Bateria: 23,5 °C de máxima. BIG 25,0 °C, MID 26,0 °C, LITTLE 26,0 °C,
  G3D 24,0 °C.
- Alertas: apenas amostras esparsas (esperado em idle). Rodada considerada
  válida como linha de base.

### Rodada 2 — caso 1 (idle)

A primeira tentativa (2026-11-10 18:58 → 21:58) foi descartada: a tela acendeu
por volta das 20:14 e o encerramento atrasou 34 min porque nada acordou o
aparelho no fim da janela. Repetida em 2026-11-11:

- Janela: 07:35:12 → 10:37:25 (3,037 h); fim previsto 10:35:12.
- Bateria 100% → 99%: 26,19 mAh, **0,329 %/h** e **8,6 mAh/h**.
- Offsets `1 1 1 1`; modo avião ligado; tela desligada em 100% do tempo; sem
  carregador e sem reboot.
- Suspensão: 1 intervalo de 3 730 s. O coletor registrou 1 amostra.
- Bateria: 20,9 °C de máxima (a rodada do caso 0 foi a 23,5 °C).

Comparação: nas horas 2 e 3 as duas rodadas consumiram exatamente os mesmos
26,19 mAh; a diferença de 7,48 mAh no total está nas duas primeiras marcas do
medidor e equivale a duas unidades do contador. Ver a nota sobre resolução
abaixo.

### Resolução da medição

O contador de carga (`charge_counter`) anda em passos de 0,1% (~3,7 mAh). Numa
janela de 3 h o consumo fica em ~9 passos, ou seja, a incerteza por rodada é da
ordem de ±6% a ±11%. Diferenças menores que isso entre casos são ruído, não
efeito do undervolt:

- caso 0: 9 passos (33,68 mAh);
- caso 1: 7 passos (26,19 mAh);
- horas 2 e 3 de ambas: 7 passos (26,19 mAh).

Se os casos ficarem empatados dentro dessa faixa, a decisão deve sair do
stress (estabilidade e temperatura) ou de uma rodada noturna mais longa.

### Decisão

Com os casos 0 e 1 empatados e nenhuma instabilidade, o usuário decidiu
encerrar a fase de medição sem rodar os casos 2 e 3: testes próprios com `3%`
nos três clusters e `1%` na GPU já vinham se mostrando estáveis. O default do
kernel foi fixado em `3 3 3 1` (o caso 3 deste protocolo), o `.ko` foi
recompilado e o boot revalidado. A escolha abre mão da comparação de consumo
entre 2 e 3 em troca de não atrasar o restante do projeto.

## Critério de escolha final

1. Descartar qualquer caso com instabilidade ou perda de pico acima do ruído.
2. Entre os estáveis, escolher o de menor consumo em idle com sustentado igual
   ou melhor.
3. Aplicar o valor escolhido como default no Kconfig, recompilar **apenas o
   `.ko`** (sem `boot.img`), instalar e revalidar boot e suspend.
4. Atualizar o documento 22, a tabela de estado e o índice; só então mergear a
   branch `undervolt-module` na `resukisu`.
