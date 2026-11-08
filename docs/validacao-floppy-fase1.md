# Validação da Fase 1 (tweaks do Floppy)

Build `#3`, gravado em 2026-11-08. Base: `resukisu` + branch `floppy-tweaks`.

## Imagem

| Item | Valor |
|---|---|
| Release | `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| `boot.img` | `boot-floppy-fase1.img` |
| SHA-256 | `92ceea674b9b5b189cb802bb03ea43585b5df311d3b7745160b37bb56b93de3e` |
| `Image` | 34.148.864 bytes (stock anterior: 30.870.016) |
| Boot anterior (rollback) | `8a21cff3bc958d50400a390423eb2113a00bbc92f29ca73625bd530c7ca225dd` |

O `release` é idêntico ao stock, então o vermagic dos 271 módulos da ROM
continua válido. CFI, PAC, shadow call stack, LTO e `MODVERSIONS` estão
habilitados.

## O que entrou

- `-mcpu=cortex-a55 -mtune=cortex-a78`.
- ThinLTO: `import-hot-multiplier=2`, `inline-threshold=300`,
  `import-instr-limit=30`. `-fsplit-lto-unit` foi mantido porque o clang
  rejeita `-fno-split-lto-unit` junto com `-fsanitize=cfi`.
- `-mllvm -unroll-threshold=600` e `-regalloc-enable-advisor=release`.
- `-funroll-loops` em `lib/lz4`, `lib/lzo` e `lib/zstd`.
- `CONFIG_PM_WAKELOCKS_GC`, `CONFIG_WQ_POWER_EFFICIENT_DEFAULT`,
  `CONFIG_STRIP_ASM_SYMS` e `CONFIG_DISABLE_TRACE_PRINTK`.
- `CONFIG_DEFAULT_BBR` (`tcp_congestion_control=bbr`).
- `CONFIG_NET_SCH_CAKE` + `CONFIG_NET_SCH_DEFAULT` + `CONFIG_DEFAULT_CAKE`
  (símbolo `DEFAULT_CAKE` adicionado ao `net/sched/Kconfig`, não existia).
- `CONFIG_IP_NF_TARGET_TTL` (seleciona `NETFILTER_XT_TARGET_HL`).
- Remoção de `dbg_snapshot_task()` do `__schedule`.
- kswapd acordado de forma exclusiva (um thread por pedido).
- Wakelock do alarmtimer reduzido de 2 s/1 s para 500 ms.
- `energy_step`: removido o `policy->rwsem` do caminho quente e adicionado
  recheck limitado de scale-up.

Os detalhes técnicos de cada item (mecanismo, compatibilidade, riscos e como
evoluir) estão em `docs/alteracoes/`.

## Resultado no aparelho

| Verificação | Resultado |
|---|---|
| `sys.boot_completed` | 1 |
| `uname -r` | `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| Módulos carregados | 271 (mesmo conjunto do stock) |
| `unknown symbol` / versão | nenhum |
| Root | `su -c id` = uid 0, contexto `u:r:ksu:s0` |
| Congestion control | `bbr` |
| Qdisc padrão | `cake` (confirmado em `wlan0`) |
| ZRAM | swap ativo (~2,1 GB, lzo-rle) |
| Frequências | little 533 MHz, prime 2,6 GHz sob carga de boot |
| `trace_printk` | aviso de boot continua, vindo de módulo vendor pré-compilado; o código built-in não chama mais |

### WARNINGs

Três WARNINGs, todas já presentes no stock e no build anterior:

- `kernel/sched/sched.h:1122` (duas ocorrências no boot; o stock também
  apresenta uma ou duas dependendo do timing).
- `mali_kbase_mem_linux.c:1238`.
- `mm/memcontrol.c:1317` (aparece depois, sob pressão de memória).

Nenhum panic, `BUG:` ou erro de CFI.

## Pendente

- Drain em idle por 8 h comparado ao build anterior.
- Suspend/resume e wakeups de longa duração.
- Uso normal por alguns dias (Wi-Fi, Bluetooth, câmera, chamadas, hotspot).
- Comparação de boot time e benchmarks antes/depois.

## Rollback

O boot anterior (`#2`) está identificado pelo hash
`8a21cff3bc958d50400a390423eb2113a00bbc92f29ca73625bd530c7ca225dd`.
O procedimento de gravação e volta está em `docs/boot-img-e-flash.md`.

## Créditos

Os patches desta fase vêm do
[FloppyKernel](https://github.com/FlopKernel-Series/flop_exynos2100_kernel):

- **Flopster101**: flags de compilação, `dbg_snapshot_task`, kswapd,
  `energy_step`, configs de energia e de rede, correção do `decon_core.c`.
- **Floppy1280**: ideia original das flags de otimização e do MLGO.
- **celtare21** e **Pranav Vashi**: relaxamento do wakelock do alarmtimer.
- **Park Ju Hyung**, **Alex Naidis** e **prabhatkrmishra**:
  `CONFIG_DISABLE_TRACE_PRINTK`.

O mapeamento completo, incluindo base, toolchain e root, está em
`docs/creditos.md`.
