# Undervolt runtime no Exynos 2100

| Campo | Valor |
|---|---|
| Status | histórico — substituído pelo [documento 22](22-undervolt-modulo-kernel.md) |
| Commit(s) | documentação apenas — o patch de kernel foi descartado |
| Arquivos | `docs/alteracoes/21-undervolt-runtime.md`, `docs/analise-floppy.md` |
| Origem | interface `percent_margin` do módulo `cmupmucal.ko` da ROM; análise do Floppy `27200b8ad` |
| Risco | baixo com offsets conservadores; médio se o offset crescer |
| Validação | `-1%` em CL0/CL1/CL2/G3D aplicado e reaplicado no boot; aparelho estável |

> Este documento registra a investigação e a primeira persistência via script.
> O uso atual é o módulo de kernel descrito em
> [22 — Undervolt por módulo de kernel](22-undervolt-modulo-kernel.md).

## Contexto

O plano da Fase 2 era portar o undervolt runtime do Floppy
(`27200b8ad`), que adiciona `SOC_EXYNOS2100_UNDERVOLT` e o sysfs
`/sys/kernel/exynos_uv/` no `drivers/soc/samsung/cal-if/fvmap.c`.

Durante a implementação, o patch aplicou e compilou, mas a verificação do
`Image` mostrou que ele **não entra no kernel**: nesta árvore
`CONFIG_CAL_IF=m`, então `fvmap.c` faz parte do módulo `cmupmucal.ko`. O
aparelho carrega esse módulo pré-compilado de `/vendor_dlkm`, que é `erofs`
somente leitura. O `Image` compilado não contém o código do patch e o sysfs
`/sys/kernel/exynos_uv/` não existiria em runtime.

O patch foi descartado. A investigação, porém, encontrou que o próprio módulo
da ROM já expõe uma interface equivalente e mais ampla.

## Mudança

Nenhuma alteração de código de kernel foi mantida.

A entrega é:

- esta documentação, registrando por que o patch do Floppy não se aplica a esta
  árvore;
- o uso da interface existente
  `/sys/kernel/percent_margin/<domínio>_margin_percent`;
- um módulo do KernelSU, fora do repositório do kernel, para reaplicar offsets
  conservadores a cada boot.

### Módulo do KernelSU

Instalado em `/data/adb/modules/undervolt-o1s/`:

`module.prop`

```text
id=undervolt-o1s
name=Undervolt o1s
version=1.0
versionCode=1
author=o1s-kernel-custom
description=Offsets conservadores de undervolt via /sys/kernel/percent_margin
```

`service.sh` (`0755`, contexto `u:object_r:ksu_file:s0`)

```sh
#!/system/bin/sh
SYS=/sys/kernel/percent_margin

for dom in cpucl0 cpucl1 cpucl2 g3d; do
    f="$SYS/${dom}_margin_percent"
    [ -w "$f" ] && echo -1 > "$f"
done
```


## Mecanismo

O `fvmap.c` do módulo cria `/sys/kernel/percent_margin/` com 19 domínios (CPU,
GPU, memória, display, câmera, modem etc.). O `store` aceita valores de `-100`
a `100` e repassa o valor assinado para `cal_dfs_set_volt_margin()`, que envia
`MARGIN_REQ` ao ACPM:

```c
if (input < -100 || input > 100)
	return -EINVAL;

vclk_id = get_vclk_id_from_margin_id(margin_id);
...
percent_margin_table[margin_id] = input;
cal_dfs_set_volt_margin(vclk_id | ACPM_VCLK_TYPE, input);
```

Valor negativo significa undervolt. O `dmesg` confirma o caminho:

```text
exynos_acpm_set_volt_margin: [CPUCL0] +margin -1 uV
```

Os valores são estado do módulo e voltam a `0` no boot; por isso a persistência
é feita por script de inicialização.

## Compatibilidade

- Nenhum arquivo de kernel, config ou módulo da ROM é alterado.
- Release string, vermagic, CFI, PAC, shadow call stack, KFENCE e SLUB_DEBUG
  permanecem como estavam.
- A interface é `0600` (root apenas).
- O módulo do KernelSU não monta arquivos de sistema; só executa um script.

## Validação

Feito no kernel da Fase 1 (`5.4.302-android12-5.4-exynos-ga273fe61262c`):

- escrita de `-1` em `cpucl0_margin_percent` aceita e revertida em seguida;
- módulo `undervolt-o1s` instalado e `service.sh` executado;
- offsets aplicados: `cpucl0=-1`, `cpucl1=-1`, `cpucl2=-1`, `g3d=-1`;
- `dmesg` mostra os quatro pedidos:

  ```text
  exynos_acpm_set_volt_margin: [CPUCL0] +margin -1 uV
  exynos_acpm_set_volt_margin: [CPUCL1] +margin -1 uV
  exynos_acpm_set_volt_margin: [CPUCL2] +margin -1 uV
  exynos_acpm_set_volt_margin: [G3D] +margin -1 uV
  ```

- aparelho estável, `sys.boot_completed=1` e root intactos;
- **persistência validada:** após reboot, o `service.sh` reaplicou os quatro
  offsets em `~14 s` de uptime, com os mesmos registros no `dmesg`;
- após o reboot o aparelho ficou na tela de bloqueio, mas o `adb` continuou com
  root (`u:r:ksu:s0`) e os 270 módulos carregaram.

**Pendente:**

- teste longo de idle/suspend com `-1%` em `CL0`, `CL1`, `CL2` e `G3D`;
- medição de drain e temperatura comparada ao build anterior.

## Como evoluir

- Subir um domínio por vez e no máximo `1%` por etapa.
- O teste mais sensível é idle longo e suspend: é onde o undervolt agressivo
  costuma causar reboot aleatório.
- Se houver instabilidade, voltar o domínio para `0` e registrar o valor.
- Evitar offsets em domínios de câmera/modem sem teste específico.
- Comparar `dumpsys thermalservice` e drain antes de aumentar.

## Rollback

Em runtime, sem reboot:

```sh
for d in cpucl0 cpucl1 cpucl2 g3d; do
  echo 0 > /sys/kernel/percent_margin/${d}_margin_percent
done
```

Para remover a persistência, apague o módulo do KernelSU:

```sh
rm -rf /data/adb/modules/undervolt-o1s
```

Não há patch de kernel para reverter.

## Créditos

**Flopster101** (`27200b8ad`, FloppyKernel) e **Floppy1280**, pela análise e pelo
trabalho original de undervolt do Exynos 2100. A interface `percent_margin`
usada em runtime é do `cmupmucal.ko` da ROM (Samsung/LineageOS). Licença
GPL-2.0. Ver `docs/creditos.md`.
