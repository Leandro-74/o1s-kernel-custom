# Créditos

Este kernel é um trabalho derivado de código GPL-2.0. Os créditos abaixo
cobrem a base, as ferramentas e cada mudança portada do FloppyKernel.

## Base e ROM

- **LineageOS** e **Mack** (`Macktherealonereally`): ROM LineageOS 23.2 para o
  Galaxy S21 e árvore `android_kernel_samsung_universal2100`
  (`lineage-23.2-20261031`, commit `a273fe61262c`, Linux 5.4.302).
- **Samsung Electronics**: código original do Exynos 2100, drivers, EMS,
  `cal-if`/`fvmap`, térmico, zram e demais drivers Samsung.
- **Linux kernel** e **Android Common Kernel**: código upstream e backports.

## Root

- **KernelSU**, de tiann e colaboradores.
- **SukiSU Ultra** e **ReSukiSU**, mantenedores e colaboradores.
- **BakaSU**, cujo driver está embutido neste kernel (commit `516d3e87`,
  driver `35257`, uapi 6).

## Toolchain e empacotamento

- **AOSP Clang `r563880c`** (21.0.0) e **LLD 21**, toolchain do kernel stock.
- **AOSP** `mkbootimg`, `unpack_bootimg` e `avbtool`.
- **Heimdall** e **Odin**, usados no rollback para a stock.

## FloppyKernel

- Repositório: <https://github.com/FlopKernel-Series/flop_exynos2100_kernel>
- Branch analisada: `floppy-main`, commit `d0309b011`.
- **Flopster101** (Nahuel Gómez) `<nahuelgomez329@gmail.com>`: mantenedor do
  Floppy e autor da maior parte dos patches usados aqui.
- **Floppy1280**: flags de otimização do compilador e configuração do MLGO,
  importadas pelo Floppy (`0c0af29ab`). O undervolt do Exynos 2100 também
  deriva do kernel dele (`flop_s5e8825_kernel`).
- **celtare21** `<celtare21@gmail.com>`: patch do alarmtimer (`53cab1dcb`),
  com sign-off de **Pranav Vashi** `<neobuddy89@gmail.com>`.
- **Park Ju Hyung** `<qkrwngud825@gmail.com>`, **Alex Naidis**
  `<alex.naidis@linux.com>` e **prabhatkrmishra** `<mprabhat774@gmail.com>`:
  opção `CONFIG_DISABLE_TRACE_PRINTK` (`ca6448e4c`).
- **Sultan Alsawaf** `<sultan@kerneltoast.com>`: Capacity Aware Superset
  Scheduler (CASS), ainda não portado.
- **andip71**: Boeffla wakelock blocker, ainda não portado.

## Fase 1 — o que foi portado

| Commit local | Origem no Floppy | Autor |
|---|---|---|
| `690cbc40d` | `e791203f8` | Flopster101 |
| `fb49f9c24` | `0c0af29ab`, `92bc2ca32`, `7323178d6` | Flopster101 / Floppy1280 |
| `8fbae7443` | `0c0af29ab`, `7323178d6` | Flopster101 / Floppy1280 |
| `e07c775c7` | `94ded34ca` | Flopster101 |
| `d1e26a1ae` | `22970adac` e configs de energia | Flopster101 |
| `a6f15406f` | config de BBR do Floppy | Flopster101 |
| `f344e290b` | `net/sched/Kconfig` do Floppy | Flopster101 + upstream Linux |
| `89742f468` | config de `IP_NF_TARGET_TTL` | Flopster101 |
| `08e67190a` | config de `STRIP_ASM_SYMS` | Flopster101 |
| `37ee11b45` | `ca6448e4c` + `fa1921857` | Park Ju Hyung, Alex Naidis, prabhatkrmishra, Flopster101 |
| `d19039923` | `4bc127568` | Flopster101 |
| `d8325bf5c` | `bbe99e5e7` | Flopster101 |
| `895a6e39a` | `53cab1dcb` | celtare21, Pranav Vashi |
| `ca145cc42` | `c882fbadc` | Flopster101 |

## Fases futuras — créditos já reservados

- **Undervolt runtime**: Flopster101 (`27200b8ad`), a partir do trabalho de
  Floppy1280.
- **Offsets térmicos por zona**: Flopster101 (`ba257f035` e correções).
- **Hotplug do prime**: Flopster101 (`a7ae7e5f6`, `6fda90728`).
- **CASS**: Sultan Alsawaf, integração com EMS por Flopster101
  (`4693764e9`).
- **MGLRU e lruvec lock**: Yu Zhao e demais autores upstream; backports do
  Floppy e do Android Common Kernel.
- **Batched TLB/MMU-GATHER**: autores upstream (Nadav Amit, Will Deacon e
  outros); backports do Floppy.
- **ZRAM multi-comp**: Flopster101 e autores upstream do zram.
- **kshrink**: Flopster101, a partir do reclaim assíncrono de Oplus/Xiaomi.
- **Boeffla wakelock blocker**: andip71.

## Licença

Todo o código derivado mantém a licença original GPL-2.0 (ou GPL-2.0-only,
quando indicado no arquivo). Nenhum código é relicenciado por este projeto.

LZ4K e LZ4KD **não** foram incluídos: o código da Huawei está marcado como
"All rights reserved" e a licença não está clara.
