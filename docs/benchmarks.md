# Benchmarks e estado do aparelho

Este documento acompanha o estado do aparelho de teste, as modificações
aplicadas e os resultados de benchmark. Ele é atualizado sempre que uma nova
coletânea de resultados é fornecida (regra 6 do `AGENTS.md`).

## Como atualizar

- Cada coletânea vira uma seção nova em **Rodadas**, com data e rótulo.
- Toda rodada precisa registrar o ambiente: build, kernel, versões dos apps,
  temperatura e carga no início e no fim, e duração.
- Dado que não foi informado fica como `não informado`; nada é estimado ou
  preenchido por dedução.
- Toda rodada entra também na tabela **Histórico**, para comparação.
- Nunca registrar serial, IMEI, MAC, SSID, número de telefone ou qualquer dado
  pessoal. Modelo, codename, build e kernel são suficientes.

## Estado atual

| Item | Valor |
|---|---|
| Modelo / codename | Galaxy S21 / `o1s` |
| ROM / build | LineageOS 23.2-20261031-UNOFFICIAL |
| Android / API | 16 / 36 |
| Kernel | `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| Root | BakaSU, driver `35257` |
| Módulos carregados | 271 (inclui `undervolt_o1s`) |
| GPU | Mali-G78 MP14, KMD e UMD `r38p0-01eac0` |
| Undervolt | módulo ativo, margem padrão de 1% |
| Última atualização | 2026-11-09 |

## Modificações realizadas

| Mudança | Documento | Status |
|---|---|---|
| Fase 1 (scheduler, mm, rede, libs, debug) | `docs/validacao-floppy-fase1.md` | mesclada em `resukisu` |
| BakaSU como submódulo, Kbuild e hooks | `docs/alteracoes/01` a `04` | validado |
| Undervolt por módulo de kernel | `docs/alteracoes/22-undervolt-modulo-kernel.md` | validado (build, runtime e boot) |

## Metodologia de benchmark

### Aplicativos

| Benchmark | Pacote | Versão |
|---|---|---|
| 3DMark (Wild Life) | `com.futuremark.dmandroid.application` | 2.7.5093 |
| Geekbench 7 | `com.primatelabs.parkdale` | 7.1.0 |
| AnTuTu v12 | `com.antutu.ABenchMark` | 12.0.3 |

### Procedimento

1. Execução manual, sempre na mesma ordem: 3DMark → Geekbench → AnTuTu.
2. Aparelho descarregando (sem carregador) e com bateria suficiente para
   concluir a suíte inteira.
3. Tela ligada, aparelho em primeiro plano e sem uso durante o teste.
4. Antes de começar: registrar temperatura e carga; fechar apps em segundo
   plano.
5. Ao terminar: registrar temperatura e carga finais e a duração de cada teste.
6. Os resultados exportados pelos próprios apps são a fonte; nenhum número é
   copiado de memória ou estimado.

### O que invalida uma rodada

- Atualização de qualquer um dos três apps no meio da série.
- Mudança de build, kernel, config ou módulo entre os testes.
- Carregador conectado ou carregamento sem fio ativo.
- Throttling térmico perceptível, notificações ou uso do aparelho durante a
  medição.
- Suíte interrompida: a rodada só é registrada se os três testes concluírem.

## Rodadas

### 2026-11-09 — estado atual (undervolt ativo)

| Item | Valor |
|---|---|
| Build / kernel | LineageOS 23.2-20261031-UNOFFICIAL / `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| Governador | `energy_step` |
| Memória | 7,03 GB |
| 3DMark Wild Life | **6 728** (FPS médio 40,29) |
| Geekbench 7 CPU | **1 257** single / **3 655** multi |
| Geekbench 7 GPU OpenCL | **6 259** |
| Geekbench 7 GPU Vulkan | **5 710** |
| AnTuTu v12 | **1 303 233** |
| Temperatura | 3DMark 26 °C → 29 °C; AnTuTu 31,8 °C → 38,3 °C; Geekbench `não informado` |
| Carga | 3DMark 100% → 100%; AnTuTu −6% |
| Duração | `não informado` |

Detalhamento do AnTuTu:

| Componente | Pontuação |
|---|---|
| CPU | 463 638 |
| GPU | 210 270 |
| Memória | 290 620 |
| UX | 338 705 |

Capturas completas em `benchmarks_screenshots/2026-11-09/`, no diretório pai do
repositório.

Observações:

- O 3DMark informa que o resultado supera 98% dos Galaxy S21 (Exynos 2100)
  registrados e 76% de todos os dispositivos.
- O AnTuTu registra 4 núcleos a 2,21 GHz, 3 a 2,81 GHz e 1 a 2,91 GHz, com
  `energy_step` como governador.
- O Vulkan ficou 8,8% abaixo do OpenCL no Geekbench; ambos usam a Mali-G78 r0p1
  (14 unidades de computação).
- Os três apps rodaram no mesmo build e com o undervolt de 1% ativo. A ordem e
  as condições estão descritas na metodologia acima.

Modelo de seção para as próximas rodadas:

```md
### AAAA-MM-DD — <rótulo>

| Item | Valor |
|---|---|
| Build / kernel | |
| 3DMark | |
| Geekbench | |
| AnTuTu | |
| Temperatura inicial / final | |
| Carga inicial / final | |
| Duração | |

Observações e análise:
```

## Histórico

| Data | 3DMark | Geekbench | AnTuTu | Build / kernel | Observações |
|---|---|---|---|---|---|
| 2026-11-09 | 6 728 | CPU 1 257 / 3 655; GPU 6 259 CL / 5 710 VK | 1 303 233 | `5.4.302-android12-5.4-exynos-ga273fe61262c` | undervolt 1% ativo; primeira rodada |

## Monitoramento de consumo

As medições de consumo usam uma ferramenta local de desenvolvimento, mantida
**fora do repositório**. Ela não é exigida por nenhuma funcionalidade do kernel:
removê-la não altera código, config nem módulos da ROM.

### Funcionamento básico

- É um módulo do KernelSU, instalado apenas no aparelho de teste.
- O `service.sh` inicia um supervisor; o supervisor reergue o coletor se ele
  for encerrado, então o monitoramento continua até o próximo boot.
- O coletor lê `sysfs` a cada 60 segundos e acrescenta uma linha ao CSV em
  `/sdcard/o1s-lab/power/`.
- Uma página WebUI mostra o resumo do agora (carga, corrente, temperaturas,
  frequências e estado da tela) e permite emitir um relatório completo sob
  demanda. Não há geração automática de relatórios.
- As amostras brutas são mantidas em CSV mensal; os relatórios são escritos em
  Markdown na mesma pasta.

### Metodologia

- **Amostragem:** 60 s. A leitura de `sysfs` é desprezível para o consumo e não
  interfere em benchmarks.
- **Classificação:** `idle` quando a tela está desligada e o aparelho não está
  carregando; `ativo` quando a tela está ligada; amostras de carregamento são
  excluídas dos cálculos de consumo.
- **Suspensão:** o coletor usa `sleep` monotônico, que não acorda o aparelho.
  Quando o sistema suspende, o coletor congela junto e retoma depois. Intervalos
  maiores que duas amostras são marcados como tempo suspenso, em vez de ter a
  corrente interpolada.
- **Resistência:** o processo roda com `oom_score_adj` ajustado e supervisor
  próprio, para não ser encerrado pelo low-memory killer.
- **Relatório:** taxa de consumo em %/h e mAh/h separada por estado, corrente
  média/mínima/máxima, temperatura média/máxima por zona, frequência média dos
  clusters e da GPU, eventos (picos de corrente, quedas abruptas, indício de
  throttling) e comparação com a janela anterior.

## Documentos relacionados

- `docs/validacao-floppy-fase1.md` — validação da Fase 1.
- `docs/alteracoes/README.md` — índice das mudanças.
- `docs/analise-floppy.md` — catálogo e roadmap.
