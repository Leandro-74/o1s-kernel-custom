# Análise do kernel Floppy e plano de otimização para o o1s

> Documento de análise. Nenhuma mudança de kernel foi feita a partir dele.
> Cada item proposto precisa ser implementado, compilado, gravado e validado
> separadamente, com commit próprio.

## 1. Resumo executivo

O Floppy é uma árvore **GKI** de 5.4.304 mantida por Flopster101, com 920
commits próprios e foco em desempenho, memória e consumo. Ele é uma ótima
**fonte de patches**, mas não serve como base: desativa mitigações que a nossa
árvore mantém (CFI, PAC, KFENCE, SLUB_DEBUG), usa padding de ABI GKI e traz
recursos de OneUI/Samsung que não existem no LineageOS.

O que vale a pena portar, em ordem de custo/benefício:

1. **Flags de compilação e configs de baixo risco** (Fase 1) — ganho imediato,
   praticamente sem chance de regressão funcional.
2. **Undervolt em runtime, offsets térmicos, Boeffla wakelock blocker e ajustes
   do EMS** (Fase 2) — ganho real de bateria/temperatura, risco controlado.
3. **CASS, MGLRU, lruvec lock e batched TLB** (Fase 3) — ganho maior, porém são
   séries grandes que tocam o núcleo de mm/sched e exigem validação longa.
4. **ZRAM multi-comp/LZ4K, kshrink e correções de mm_users** (Fase 4) — porte
   grande ou licença duvidosa; deixar por último.

O que **não** deve ser portado: padding GKI, fake uname/BPF spoof,
DEFAULT_SUPPORT_ONEUI, Mali version selector, mass storage hack, LZ4K/LZ4KD
(copyright Huawei "All rights reserved") e qualquer coisa que desligue CFI/PAC.

## 2. O que é o Floppy (fatos verificados)

| Item | Valor |
|---|---|
| Repositório | `FlopKernel-Series/flop_exynos2100_kernel` |
| Branch | `floppy-main` (HEAD `d0309b011`, 2026-11-05) |
| Versão | 5.4.304 |
| Base | GKI (Android Common Kernel) + drivers Samsung |
| Config | `arch/arm64/configs/exynos2100-unified_defconfig` (7333 linhas) |
| Configs auxiliares | `ksu.config`, `sukisu.config`, `xxksu.config`, `droidspaces.config`, `nethunter.config` |
| Toolchain | clang 23.0.1 (r614150), ThinLTO, sem CFI/PAC |
| Commits próprios | 920 (Flopster101 é o autor) |
| LOCALVERSION | `-Floppy-V-` + `LOCALVERSION_AUTO=y` |
| EMS | `CONFIG_SCHED_EMS=m` (módulo) |
| Extra | 117 símbolos de config habilitados só nele |

## 3. Diferenças estruturais que limitam o porte

| Tema | Floppy | Nosso kernel | Consequência |
|---|---|---|---|
| Base | GKI + drivers Samsung | Samsung common (`universal2100`) | Patches de core precisam ser adaptados |
| EMS | módulo (`=m`) + vendor hooks | built-in (`=y`), chamado direto de `select_task_rq()` | CASS exige adaptação do caminho fair |
| Hardening | sem CFI/PAC/KFENCE/SLUB_DEBUG | CFI+shadow call stack+PAC+KFENCE+SLUB_DEBUG | Não copiar config de hardening |
| ABI | `TASK_STRUCT_GKI_ABI_PAD`, `MODULE_STRUCT_GKI_ABI_PAD`, `ARM64_GKI_MEM_ABI_SYMBOLS` | não existem | Não portar; mudariam layout de structs |
| Módulos | próprios, todos recompilados | 271 módulos pré-compilados da ROM + `MODVERSIONS=y` | Não alterar structs/símbolos exportados |
| ZRAM | multi-comp + LZ4K/LZ4KD + auto size | zram legado + LRU writeback | Porte grande (~3 mil linhas) |
| MM | MGLRU + kshrink + lruvec lock + TLBBATCH | LRU clássico | Porte grande, requer pré-requisitos |
| Versão | 5.4.304 | 5.4.302 (`a273fe61262c`) | Patches dependem de contexto; testar cada um |

## 4. Catálogo de personalizações

Legenda de risco: **B** baixo, **M** médio, **A** alto.
"Estado" = o que já existe na nossa árvore/config.

### 4.1 Compilação e LTO

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `-mcpu=cortex-a55 -mtune=cortex-a78` | Gera código para o ISA do A55 com agendamento do A78 | ausente (genérico armv8-a) | B | Portar |
| `-flto=thin -funified-lto -fno-split-lto-unit` | ThinLTO em partição única, menos overhead de link | `-flto=thin -fsplit-lto-unit` | B | Portar |
| `-mllvm -import-hot-multiplier=2` | Importa mais funções quentes entre TUs | ausente | B | Portar |
| `-mllvm -inline-threshold=300` | Inlining mais agressivo | padrão | B | Portar |
| `-mllvm -import-instr-limit=30` | Limite de importação entre TUs | 5 | B | Portar |
| `-mllvm -unroll-threshold=600` | Desenrola laços maiores | ausente | B | Portar |
| `-mllvm -regalloc-enable-advisor=release` | Alocador de registradores modo release | ausente | B | Portar |
| `-funroll-loops` em `lib/lz4`, `lib/lzo`, `lib/zstd` | Compressão/descompressão mais rápida | só `-O3` | B | Portar |
| `CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE_O3` | `-O3` global | `-O2` (`FOR_PERFORMANCE`) | M | Avaliar; tende a inflar imagem |
| Polly | otimizador polinomial | — | M | Floppy **reverteu**; não portar |

Todas as flags acima foram testadas com o nosso clang `r563880c` (21.0.0) e são
aceitas, inclusive `-mcpu=cortex-a55 -mtune=cortex-a78` com
`--target=aarch64-linux-gnu`.

### 4.2 Scheduler

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `SCHED_CASS` (`kernel/sched/cass.c`, 321 linhas) | Substitui a escolha de CPU de tarefas CFS por um algoritmo capacity-aware | ausente | M/A | Fase 3 |
| `SCHED_EMS_CASS_INTEGRATION` | Faz o EMS devolver `-1` para fair e deixar o CASS decidir | ausente | M | Junto com CASS |
| `4bc127568` remove `dbg_snapshot_task()` do `__schedule` | Remove chamada inútil a cada troca de contexto | presente (`core.c:4381`) | B | Portar |
| `c882fbadc` EMS `energy_step`: remove rwsem do caminho quente | Menos contenção com sysfs/thermal e scale-up mais rápido | patch aplica limpo | M | Fase 1/2 |
| `a7ae7e5f6` atraso de 3 s para desligar o prime | Evita hotplug oscilando | ausente | M | Fase 2 |
| `6fda90728` atraso do hotplug do prime até `boot_completed` | Evita hotplug durante o boot | ausente | M | Fase 2 |
| `60f098ffb` `update_curr_rt()` no dequeue RT | Corrige contabilidade de tempo RT | não aplica (contexto) | M | Avaliar manualmente |
| Reverts de afinidade de IRQ | Afinidade fixa de IRQ no little | não aplica | M | Não portar |
| `SCHED_USE_FLUID_RT` | RT fluidificado | já ativo | — | — |

O CASS é o item de maior impacto do Floppy no scheduler, mas o nosso EMS é
built-in e escolhe CPU para fair **antes** do callback da classe. O porte exige:
incluir `cass.c`/`cass_rt.h`, trocar `select_task_rq_fair` no `fair.c`,
compilar com `CONFIG_SCHED_CASS=y` e fazer `exynos_select_task_rq_fair()`
devolver `-1` para fair quando o CASS estiver ativo.

### 4.3 Memória

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| MGLRU (`LRU_GEN`, `LRU_GEN_ENABLED`) | LRU multi-geração; menos latência de reclaim e melhor uso de RAM | ausente; `vmscan.c` difere em ~3316 linhas | A | Fase 3 |
| lruvec lock (pré-requisito do MGLRU) | Troca `zone_lru_lock` por lock por lruvec | ausente | A | Fase 3 |
| `KSHRINK_LRUVECD` / `KSHRINK_SLABD` | Reclaim assíncrono de páginas/slab via kthreads | ausente | A | Fase 4 |
| `PAGE_EXTENSION` | Metadados extra por página (usado pelo kshrink) | existe, desligado | M | Só se for usar kshrink |
| MMU-GATHER/TLBBATCH (7 commits: `41cbc2f21`, `4682e58b4`, `362a745eb`, `dac582cac`, `dc553d728`, `5b2cb9b83`, `014be827d`) | Adia e agrupa TLBIs no unmap/reclaim | ausente | M/A | Fase 3 |
| `870659a58` reparo de `mm_users` | Corrige refcount desbalanceado vindo de módulo | ausente | M | Fase 3/4 |
| `bbe99e5e7` um único kswapd por pedido | Evita vários kswapd rodando o mesmo scan | patch aplica limpo | B | Fase 1 |
| `CONFIG_MM_USERS_REFCNT_REPAIR` | Kill switch do reparo acima | ausente | M | Junto |

### 4.4 ZRAM e compressão

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `ZRAM_MULTI_COMP` | Permite algoritmo primário + secundário (ex.: lzo-rle + zstd) | ausente; zram difere em ~3069 linhas | A | Fase 4 |
| Backends LZ4/LZ4HC/LZO/ZSTD | Reescrita do `zcomp` em backends plugáveis | ausente | A | Fase 4 |
| LZ4K/LZ4KD | Codec Huawei | ausente | A | **Não portar** (licença) |
| `ZRAM_SIZE_AUTO` | Dimensiona zram em 50% da RAM | ausente | B/M | Fase 2 (config própria) |
| `ZRAM_LRU_WRITEBACK` | Writeback por LRU | já ativo | — | — |

### 4.5 Energia, suspend e wakelocks

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `PM_WAKELOCKS_GC=y` | Coleta wakelocks de userspace não usados | opção existe, desligada | B | Fase 1 |
| `WQ_POWER_EFFICIENT_DEFAULT=y` | Workqueues não pinadas usam CPU econômica | opção existe, desligada | B | Fase 1 |
| `SUSPEND_SKIP_SYNC=y` | Pula `sync` no suspend | opção existe, desligada | M | Fase 2; avaliar risco de dados |
| Boeffla wakelock blocker (`drivers/base/power/boeffla_wl_blocker.c` + hook em `wakeup.c`, ~250 linhas) | Bloqueia wakelocks por nome via sysfs | ausente | M | Fase 2 |
| `53cab1dcb` relaxa wakelock do alarmtimer | Reduz wakeups de alarmes | patch aplica limpo | B | Fase 1 |
| `CONFIG_PM_WAKELOCKS_GC` no defconfig | Ativa o GC | desligado | B | Fase 1 |

### 4.6 Térmico e undervolt

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `SOC_EXYNOS2100_UNDERVOLT` + `27200b8ad` | Offset percentual por domínio (CL0/CL1/CL2/GPU) via `/sys/kernel/exynos_uv` | patch aplica, mas `CAL_IF=m` coloca o `fvmap.c` no `cmupmucal.ko` da ROM, fora do `Image` | M | **Descartado**; usar `/sys/kernel/percent_margin` |
| Defaults de 1% por cluster | Valores que o Floppy considera estáveis | — | M | Fase 2, começando em 0% |
| Offsets térmicos por zona (`ba257f035` + 9 correções) | Antecipa/atrasa trips de throttling por zona | ausente; série de ~420 linhas | M | Fase 2 |
| `f8375d99b` anti-thrashing do hotplug ambiente | Evita liga/desliga térmico | ausente | M | Fase 2 |
| `CPU_FREQ_GOV_ENERGYSTEP` | Governor do EMS | presente | — | — |

O undervolt é o item de maior retorno, mas o patch do Floppy não se aplica a esta
árvore: como `CONFIG_CAL_IF=m`, o `fvmap.c` compila no `cmupmucal.ko`, que a ROM
carrega de `vendor_dlkm`. A interface existente
`/sys/kernel/percent_margin/<domínio>_margin_percent` já aceita offsets
negativos; é por ela que o undervolt deve ser usado. Continua sendo o item que
mais exige teste: undervolt agressivo causa reboot aleatório em idle,
exatamente o problema que o Floppy relatou e resolveu baixando para 1%.

### 4.7 I/O e filesystem

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `MQ_SSG_DEFAULT` | Torna o SSG o scheduler padrão (choice que não existe no nosso Kconfig) | `MQ_IOSCHED_SSG=m`, sem choice | B | Opcional; a ROM escolhe em runtime |
| `MQ_IOSCHED_SSG_CGROUP` | Suporte a cgroup no SSG | ausente | M | Não portar |
| Tweaks F2FS (`F2FS_SEC_*`, `F2FS_STRICT_BUG_ON`) | Recursos Samsung/OneUI | ausentes | M | Não portar |
| BTRFS/ksmbd/SMB server | Features de desktop | ausentes | B | Fora de escopo |
| `TMPFS_XATTR` | xattrs em tmpfs | ausente | B | Opcional (containers) |

### 4.8 Rede

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `DEFAULT_BBR` / `DEFAULT_CAKE` | BBR como congestion control e CAKE como qdisc padrão | BBR compilado, padrão cubic; CAKE compilado, sem choice | B | Fase 1 |
| `NET_SCH_DEFAULT` + `DEFAULT_CAKE` | Permite escolher CAKE como qdisc padrão | choice existe, desligada | B | Fase 1 |
| `IP_NF_TARGET_TTL` | Alvo TTL para tethering | opção existe, desligada | B | Fase 1 |

### 4.9 Overhead de debug

| Item | O que faz | Estado | Risco | Decisão |
|---|---|---|---|---|
| `CONFIG_DISABLE_TRACE_PRINTK` (`ca6448e4c`) | Torna `trace_printk` no-op; economiza RAM e evita warning de boot | ausente | B | Fase 1 |
| `STRIP_ASM_SYMS=y` | Remove símbolos `.Lxxx` do binário | opção existe, desligada | B | Fase 1 |
| `MODULE_FORCE_LOAD/UNLOAD` | Força carga/descarga de módulo | existe, desligado | B | Opcional |
| `SECTION_MISMATCH_WARN_ONLY` | Só avisa sobre mismatch de seção | ausente | B | Não necessário |

### 4.10 Itens específicos do Floppy que NÃO devemos portar

| Item | Motivo |
|---|---|
| `TASK_STRUCT_GKI_ABI_PAD`, `MODULE_STRUCT_GKI_ABI_PAD`, `ARM64_GKI_MEM_ABI_SYMBOLS` | Mudam layout de structs; nossos módulos pré-compilados quebrariam |
| `FAKE_UNAME_*` + `uname_bpf_spoof` | Mascara versão do kernel para BPF; evasão/teste, não otimização |
| `DEFAULT_SUPPORT_ONEUI`, `SEC_*`, `SB_*`, `SEC_DETECT`, `SEC_DUMP` | Camada OneUI/Samsung ausente no LineageOS |
| `FLOPPY_MASS_STORAGE_HACK` | Workaround de USB mass storage para OneUI |
| `MALI_VERSION_SELECTOR` + Mali r49p1/r54p1 | Nosso vendor_dlkm já traz o KMD da ROM |
| `LZ4K_COMPRESS`/`LZ4KD_COMPRESS` | Código Huawei "All rights reserved", sem licença clara |
| Desligar `CFI_CLANG`, `ARM64_PTR_AUTH`, `KFENCE`, `SLUB_DEBUG` | Perda de segurança/diagnóstico; ganho pequeno |
| IRQ affinity reverts | Fixam IRQ no little; podem piorar latência |
| `LOCALVERSION_AUTO`, `FAKE_UNAME` | Mudariam a string de release esperada pela ROM |

## 5. Roadmap proposto

### Fase 1 — Baixo risco, ganho rápido

> Implementada e validada no aparelho. Ver `docs/validacao-floppy-fase1.md`.

Tudo config/flag ou patch pequeno e já testado como aplicável.

| # | Mudança | Arquivo | Validação |
|---|---|---|---|
| 1.1 | Flags de compilação (`-mcpu/-mtune`, ThinLTO, unroll, regalloc, funroll nas libs) | `Makefile`, `lib/*/Makefile` | boot + 271 módulos + CFI ok |
| 1.2 | `PM_WAKELOCKS_GC`, `WQ_POWER_EFFICIENT_DEFAULT`, `STRIP_ASM_SYMS` | defconfig | boot + `dmesg` |
| 1.3 | `DEFAULT_BBR`, `NET_SCH_DEFAULT`+`DEFAULT_CAKE`, `IP_NF_TARGET_TTL` | defconfig | `sysctl net.ipv4.tcp_congestion_control`, `tc qdisc` |
| 1.4 | `DISABLE_TRACE_PRINTK` (`ca6448e4c`) | `include/linux/kernel.h`, `kernel/trace/Kconfig` | boot sem warning de `trace_printk` |
| 1.5 | Remover `dbg_snapshot_task()` (`4bc127568`) | `kernel/sched/core.c` | boot + uso normal |
| 1.6 | kswapd com wakeup exclusivo (`bbe99e5e7`) | `mm/vmscan.c` | pressão de memória + `vmstat` |
| 1.7 | alarmtimer relax (`53cab1dcb`) | `kernel/time/alarmtimer.c` | idle drain |
| 1.8 | `energy_step` sem rwsem (`c882fbadc`) | `kernel/sched/ems/energy_step.c` | frequências/thermal sob carga |

Critério de aceite: `sys.boot_completed=1`, 271/271 módulos, root funcionando,
sem panic/CFI/`BUG:`, mesma string de release.

### Fase 2 — Energia, térmico e I/O

| # | Mudança | Esforço | Risco |
|---|---|---|---|
| 2.1 | Undervolt runtime — implementado em `undervolt_o1s.ko` sem portar `27200b8ad`; ver `docs/alteracoes/22-undervolt-modulo-kernel.md` | 3 commits | B |
| 2.2 | Série de offsets térmicos (10 commits, porte manual) | 2–3 commits | M |
| 2.3 | Boeffla wakelock blocker + hook em `wakeup.c` | 2 commits | M |
| 2.4 | `SUSPEND_SKIP_SYNC` (avaliar integridade de dados) | 1 commit | M |
| 2.5 | Hotplug do prime: atraso de 3 s e após `boot_completed` | 2 commits | M |
| 2.6 | `ZRAM_SIZE_AUTO` próprio (sem multi-comp) | 1 commit | B |

Critério de aceite: 24 h sem reboot, drain em idle comparável ou melhor,
temperatura sob carga igual ou menor, nenhum wakelock essencial bloqueado.

### Fase 3 — Núcleo de memória e scheduler

Ordem obrigatória: lruvec lock → MGLRU → TLBBATCH → CASS.

| # | Mudança | Esforço | Risco |
|---|---|---|---|
| 3.1 | lruvec lock (swap.c/rmap.c/vmscan.c) | série grande | A |
| 3.2 | MGLRU (38 commits, ~3,3 mil linhas) | série grande | A |
| 3.3 | Batched TLB/MMU-GATHER (7 commits) | média | M/A |
| 3.4 | Reparo de `mm_users` | pequeno | M |
| 3.5 | CASS + integração EMS | média | M |

Critério de aceite: 72 h de uso, 271 módulos carregados, `MemAvailable` e
latência de app melhores, sem reclaim patológico, sem regressão de bateria.

### Fase 4 — Portes grandes ou duvidosos

- ZRAM multi-comp + backends (sem LZ4K/LZ4KD).
- kshrink (depende de `PAGE_EXTENSION` e conversa com MGLRU).
- Tweaks F2FS, SMB server, USB audio, etc. (features, não otimização).
- LZ4K/LZ4KD: só com licença esclarecida.

## 6. Evidências de aplicabilidade (testadas nesta árvore)

Aplicam limpo com `git apply --check`:

```
4bc127568  sched: core: Remove dbg_snapshot_task() from __schedule
53cab1dcb  kernel: alarmtimer: Relax wakelock
bbe99e5e7  vmscan: wake a single kswapd thread per request
c882fbadc  sched/ems: energy_step: remove rwsem bottleneck
27200b8ad  fvmap: runtime Exynos 2100 GPU/CPU undervolting
```

Ressalva: `27200b8ad` aplica no texto, mas **não entra no `Image`**. Com
`CONFIG_CAL_IF=m`, o `fvmap.c` pertence ao `cmupmucal.ko`, carregado de
`/vendor_dlkm` (erofs, somente leitura). O undervolt runtime deve usar
`/sys/kernel/percent_margin`, como descrito em
`docs/alteracoes/21-undervolt-runtime.md`.

Não aplicam (precisam de adaptação manual, contexto diferente):

```
60f098ffb  sched/rt: update_curr_rt() no dequeue
33bde727c  IRQ affinity (não portar)
f96f31318  IRQ affinity (não portar)
ba257f035  thermal: per-zone offsets
a7ae7e5f6  sched/ems: prime hotplug delay
6fda90728  sched/ems: delay prime hotplug until boot completed
```

## 7. Riscos transversais

1. **Módulos pré-compilados**: qualquer mudança de struct exportada ou de
   símbolo usado por módulo pode quebrar a ROM. Depois de cada fase, conferir
   `dmesg` por `unknown symbol`, `disagrees about version` e falha de
   verificação, além dos 271 módulos.
2. **MGLRU e `struct lruvec`**: o MGLRU adiciona campos em `mmzone.h`. Testar
   carga de todos os módulos que tocam mm antes de considerar a fase concluída.
3. **Undervolt**: validar em idle, em suspend e sob carga; 1% já causou reboot
   em pelo menos um aparelho no histórico do Floppy. Default inicial 0%.
4. **SUSPEND_SKIP_SYNC**: pode perder dados se o sistema morrer durante o
   suspend; manter só se a estabilidade se provar.
5. **Rollback**: cada fase gera um `boot.img` identificável, com hash
   registrado, e o procedimento de volta para a stock já documentado.

## 8. Checklist de validação (resumo)

Por build:

- [ ] `sys.boot_completed=1` e boot sem panic/CFI/`BUG:`.
- [ ] `uname -r` com a string esperada.
- [ ] 271/271 módulos carregados, zero erros de símbolo/versão.
- [ ] Root (BakaSU) funcionando no app e em `adb shell` (`su -c id`).
- [ ] `dmesg` sem WARNING novo em relação ao baseline.
- [ ] ZRAM montado e `zramctl` sem erros.
- [ ] Suspend/resume, Wi-Fi, Bluetooth, câmera, chamadas, hotspot e tethering.
- [ ] Térmico: `dumpsys thermalservice` e uso sob carga sem throttling anormal.
- [ ] Bateria: drain em idle por 8 h comparado ao build anterior.
- [ ] Benchmarks de referência (Geekbench/AnTuTu) e tempo de abertura de apps.

Procedimento detalhado já existente: `docs/validacao-o1s-2026-11-08.md`,
`docs/boot-img-e-flash.md`, `docs/build-bakasu-o1s.md`.

## 9. Recomendação

Começar pela **Fase 1**, em commits pequenos e independentes, medindo antes e
depois. Se o ganho for perceptível e a estabilidade se mantiver, seguir para a
Fase 2. As fases 3 e 4 só depois de a Fase 2 estar estável por vários dias.

## 10. Créditos

Esta análise e os patches portados não seriam possíveis sem o trabalho de:

- **Flopster101** (Nahuel Gómez), mantenedor do
  [FloppyKernel](https://github.com/FlopKernel-Series/flop_exynos2100_kernel),
  autor da maior parte dos patches de scheduler, memória, térmico, undervolt
  e configuração.
- **Floppy1280**, pelas flags de otimização e pelo trabalho original de
  undervolt do Exynos 2100.
- **celtare21** e **Pranav Vashi**, pelo patch do alarmtimer.
- **Park Ju Hyung**, **Alex Naidis** e **prabhatkrmishra**, pela opção
  `CONFIG_DISABLE_TRACE_PRINTK`.
- **Sultan Alsawaf**, pelo CASS.
- **andip71**, pelo Boeffla wakelock blocker.
- **Samsung**, **LineageOS** e **Mack**, pela base do kernel e da ROM.
- **KernelSU**, **SukiSU Ultra**, **ReSukiSU** e **BakaSU**, pelo root.

A lista completa, com o mapeamento de cada commit local para a origem, está em
`docs/creditos.md`.
