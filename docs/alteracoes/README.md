# Alterações do kernel o1s

Esta pasta guarda **um documento por mudança** de código, configuração ou build.
O objetivo é permitir que outra sessão (ou outro modelo) entenda o que foi feito,
por que foi feito, o que já foi validado e o que precisa ser conferido antes de
mexer no assunto de novo.

> Leia também o `AGENTS.md` na raiz do repositório. Ele define as regras de
> trabalho, o fluxo de branches e o que não pode ser alterado.

## Estado do projeto

| Item | Valor |
|---|---|
| Base upstream | `a273fe61262c` — tag `lineage-23.2-20261031`, Linux 5.4.302 |
| Branch padrão | `resukisu` |
| HEAD da Fase 1 | `4cb891642` (antes dos commits de documentação) |
| Root | BakaSU `516d3e87` (driver `35257`, uapi 6) |
| Build validado | `#3`, imagem `boot-floppy-fase1.img` |
| Release esperado | `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| Módulos da ROM | 271, todos carregando |

A Fase 1 do plano do Floppy foi desenvolvida na branch `floppy-tweaks`
(commits `690cbc40d`..`ca145cc42`), mesclada em `resukisu` e a branch foi
removida (local e remota). Os testes de longa duração seguem na `resukisu`; se
surgir inconsistência, crie uma branch de correção com nome descritivo.

## Como ler

Cada documento segue o mesmo formato:

1. **Cabeçalho** — status, commits, arquivos, origem e risco.
2. **Contexto** — qual problema a mudança resolve.
3. **Mudança** — o que foi alterado, com trechos de código quando útil.
4. **Mecanismo** — por que funciona e qual é o custo.
5. **Compatibilidade** — o que não pode quebrar (ABI, módulos, CFI, release).
6. **Validação** — o que já foi testado e o que falta.
7. **Como evoluir** — armadilhas e pontos de ajuste.
8. **Rollback** — como desfazer com segurança.
9. **Créditos** — autor/origem do patch.

## Índice

| # | Documento | Mudança | Commit(s) | Status |
|---|---|---|---|---|
| 01 | [BakaSU como submódulo](01-bakasu-submodulo.md) | Submódulo `KernelSU` fixado em `516d3e87` | `ab62a7d55` | validado |
| 02 | [BakaSU no Kbuild](02-bakasu-kbuild.md) | Symlink `drivers/kernelsu` + entrada no Kconfig/Makefile | `b5b15e253` | validado |
| 03 | [Hooks manuais do BakaSU](03-bakasu-hooks.md) | Hooks em `exec`, `faccessat`, `stat` e `reboot` | `f35056027` | validado |
| 04 | [Config do BakaSU e kprobes](04-bakasu-config-kprobes.md) | `CONFIG_KSU`, `KSU_MANUAL_HOOK` e decisão sobre `KPROBES` | `f8c662c02`, `bda28371e` | validado |
| 05 | [Formato do log de OOM no HPA](05-hpa-log-oom.md) | `%hd` → `%d` no `hpa_killer` | `deb565caa` | implementado |
| 06 | [CPU do Exynos 2100](06-cpu-exynos2100.md) | `-mcpu=cortex-a55 -mtune=cortex-a78` | `690cbc40d` | validado |
| 07 | [ThinLTO e inlining](07-thinlto-inlining.md) | Limites de importação/inlining do ThinLTO | `fb49f9c24` | validado |
| 08 | [Unroll e regalloc](08-unroll-regalloc.md) | `unroll-threshold` e `regalloc-enable-advisor` | `8fbae7443` | validado |
| 09 | [Unroll nas libs de compressão](09-libs-unroll.md) | `-O3 -funroll-loops` em lz4, lzo e zstd | `e07c775c7` | validado |
| 10 | [Energia: wakelocks e workqueues](10-energia-wakelocks-workqueues.md) | `PM_WAKELOCKS_GC` e `WQ_POWER_EFFICIENT_DEFAULT` | `d1e26a1ae` | parcial |
| 11 | [Rede: BBR](11-rede-bbr.md) | BBR como congestion control padrão | `a6f15406f` | validado |
| 12 | [Rede: CAKE](12-rede-cake.md) | `DEFAULT_CAKE` e CAKE como qdisc padrão | `f344e290b` | validado |
| 13 | [Rede: TTL para tethering](13-rede-ttl.md) | `IP_NF_TARGET_TTL` | `89742f468` | parcial |
| 14 | [Strip de símbolos do assembler](14-debug-strip-asm.md) | `CONFIG_STRIP_ASM_SYMS` | `08e67190a` | validado |
| 15 | [Desativar `trace_printk`](15-trace-printk.md) | `CONFIG_DISABLE_TRACE_PRINTK` + correção no `decon_core.c` | `37ee11b45` | validado |
| 16 | [Remover snapshot do scheduler](16-sched-dbg-snapshot.md) | Remove `dbg_snapshot_task()` do `__schedule` | `d19039923` | validado |
| 17 | [kswapd com wakeup exclusivo](17-mm-kswapd-exclusivo.md) | `prepare_to_wait_exclusive` no `vmscan.c` | `d8325bf5c` | parcial |
| 18 | [Wakelock do alarmtimer](18-time-alarmtimer.md) | Reduz o wakeup de 2 s/1 s para 500 ms | `895a6e39a` | parcial |
| 19 | [`energy_step` sem rwsem](19-sched-energy-step.md) | Remove `policy->rwsem` e reordena o scale-up | `ca145cc42` | parcial |
| 20 | [Documentação do projeto](20-documentacao.md) | Build, flash, análises, validação e créditos | vários | — |
| 21 | [Undervolt runtime](21-undervolt-runtime.md) | Offsets por domínio via `/sys/kernel/percent_margin` (módulo da ROM) | documentação | substituído pelo 22 |
| 22 | [Undervolt por módulo de kernel](22-undervolt-modulo-kernel.md) | Módulo `undervolt_o1s.ko`, sysfs `exynos_uv` e painel KernelSU | `0f9e42a50`, `5bcdcd913`, `dae25faee`, `25699e292`, `a68ae6335`, `2de81995f` | validado; prova física nos PMICs e painel WebUI; default `3 3 3 1`, teto de 10% |

Status:

- **validado**: passou por boot no aparelho e pelas verificações da Fase 1.
- **parcial**: implementado e compilado, mas depende de teste longo (drain,
  suspend, tethering real, pressão de memória) ou de medição comparativa.
- **implementado**: compila e não altera comportamento em runtime de forma
  perceptível; falta apenas registro de validação.

## Mudanças ainda não implementadas

As fases 2 a 4 do `docs/analise-floppy.md` continuam pendentes. Antes de
começar qualquer item:

1. Crie uma branch de teste com nome descritivo (ex.: `undervolt-runtime`).
2. Leia o item correspondente no `docs/analise-floppy.md`.
3. Siga o template `_template.md` e crie o documento da mudança **no mesmo
   commit** da implementação.
4. Valide no aparelho e só então faça o merge e apague a branch.

## Documentos relacionados

Caminhos relativos à raiz do repositório:

- `docs/build-bakasu-o1s.md` — toolchain, config e build manual.
- `docs/boot-img-e-flash.md` — empacotamento, gravação e rollback.
- `docs/benchmarks.md` — estado do aparelho, modificações e resultados de
  benchmark.
- `docs/protocolo-undervolt.md` — como os casos de undervolt são validados.
- `docs/validacao-o1s-2026-11-08.md` — validação do kernel com BakaSU.
- `docs/validacao-floppy-fase1.md` — validação da Fase 1 e imagem `#3`.
- `docs/analise-floppy.md` — catálogo do Floppy e roadmap das fases.
- `docs/creditos.md` — créditos completos e mapa commit local → origem.
- `PLAN.md` no diretório pai do repositório — plano original do projeto.
