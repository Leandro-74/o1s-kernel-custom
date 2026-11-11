# Undervolt por módulo de kernel

| Campo | Valor |
|---|---|
| Status | validado (build, runtime e boot); default `3 3 3 1` definido |
| Commit(s) | `0f9e42a50`, `5bcdcd913`, `dae25faee`, `f519b1140`, `25699e292` (branch `undervolt-module`) |
| Arquivos | `drivers/soc/samsung/undervolt/`, `drivers/soc/samsung/{Kconfig,Makefile}`, `arch/arm64/configs/samsung/o1s.config`, `ksu-modules/undervolt-o1s/` |
| Origem | autoral, sobre os símbolos exportados por `cmupmucal.ko`; abordagem inspirada no Floppy `27200b8ad` |
| Risco | baixo com offsets conservadores; médio se o offset crescer |
| Validação | `make modules`, `insmod` manual, sysfs, `dmesg`, `rmmod` e reboot |
| Protocolo | [Protocolo de validação do undervolt](../protocolo-undervolt.md) |

## Contexto

O undervolt do Exynos 2100 já é aplicado por código de kernel: o
`cmupmucal.ko` recebe uma margem percentual e envia `MARGIN_REQ` ao ACPM. O
problema era a propriedade e a automação:

- nesta árvore `CONFIG_CAL_IF=m`, então o `fvmap.c` faz parte do
  `cmupmucal.ko` da ROM, carregado de `/vendor_dlkm` (erofs, somente leitura);
- um patch no `Image` não alcança esse código;
- a interface `/sys/kernel/percent_margin/` existe, mas os valores voltam a
  zero no boot e a persistência dependia de um script de userspace.

O documento [21](21-undervolt-runtime.md) registra a investigação que descartou
o patch do Floppy e validou a interface `percent_margin`. Esta mudança coloca a
lógica em um módulo de kernel próprio, sem substituir o módulo da ROM.

## Mudança

Novo módulo in-tree `undervolt_o1s.ko` em
`drivers/soc/samsung/undervolt/`, habilitado por
`CONFIG_SOC_EXYNOS2100_UV=m`.

Kconfig:

```text
CONFIG_SOC_EXYNOS2100_UV=m
CONFIG_SOC_EXYNOS2100_UV_CL0=3
CONFIG_SOC_EXYNOS2100_UV_CL1=3
CONFIG_SOC_EXYNOS2100_UV_CL2=3
CONFIG_SOC_EXYNOS2100_UV_GPU=1
```

Sysfs, modo `0600`:

```text
/sys/kernel/exynos_uv/cpucl0_uv_percent   (CPUCL0, little)
/sys/kernel/exynos_uv/cpucl1_uv_percent   (CPUCL1, big)
/sys/kernel/exynos_uv/cpucl2_uv_percent   (CPUCL2, prime)
/sys/kernel/exynos_uv/gpu_uv_percent      (G3D)
```

O valor é a porcentagem de undervolt (positivo). `0` desliga o offset; a faixa
aceita é `0..100`.

O módulo do KernelSU em `ksu-modules/undervolt-o1s/` contém:

- `undervolt_o1s.ko` (copiado do build, não versionado);
- `service.sh` que espera o `cmupmucal` e faz `insmod`;
- `module.prop`;
- `webroot/index.html` com o painel inicial;
- `package.sh`, que monta `out/ksu-modules/undervolt-o1s.zip`.

## Mecanismo

Fluxo completo:

```text
WebUI / shell
    -> /sys/kernel/exynos_uv/<dominio>_uv_percent
    -> undervolt_o1s.ko
    -> cal_dfs_set_volt_margin(vclk_id | ACPM_VCLK_TYPE, -percent)
    -> cmupmucal.ko
    -> ACPM (MARGIN_REQ)
```

No `module_init`, o módulo:

1. percorre a lista de `vclk` do ACPM com `cmucal_get_list_size()` e
   `cmucal_get_node()`, ambos exportados pelo `cmupmucal`;
2. guarda o índice do `vclk` cujo `margin_id` corresponde a `MARGIN_CPUCL0`,
   `MARGIN_CPUCL1`, `MARGIN_CPUCL2` e `MARGIN_G3D`;
3. cria o kobject `/sys/kernel/exynos_uv/`;
4. aplica os defaults de Kconfig diferentes de zero.

No `store`, o valor é validado em `0..100` e convertido para margem negativa. No
`module_exit`, os quatro domínios recebem `0` antes de remover o sysfs.

O `service.sh` roda no `late_start` do KernelSU, espera
`/sys/kernel/percent_margin` (sinal de que o `cmupmucal` carregou), confere o
`lsmod` e faz `insmod`. O `.ko` fica no diretório do módulo com contexto
`u:object_r:ksu_file:s0`.

### Build e empacotamento

```sh
export PATH="$PWD/../toolchains/bin:$PWD/../toolchains/clang-r563880c/bin:$PATH"
LOCALVERSION= make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- modules
./ksu-modules/undervolt-o1s/package.sh
```

O `.ko` precisa ser recompilado sempre que o kernel mudar. `CONFIG_MODVERSIONS`
faz o carregamento falhar se os CRCs não baterem; o vermagic precisa continuar
`5.4.302-android12-5.4-exynos-ga273fe61262c`.

## Compatibilidade

- Nenhum arquivo do `Image` é alterado: `CONFIG_CAL_IF` continua `m` e o
  `cmupmucal.ko` da ROM permanece o dono do ACPM.
- Release string, vermagic, CFI, PAC, shadow call stack, KFENCE e SLUB_DEBUG
  permanecem iguais.
- Nenhum layout de struct ou assinatura de símbolo exportado muda.
- O módulo só importa símbolos `EXPORT_SYMBOL_GPL` já existentes.
- O sysfs é `0600` (root apenas).
- Sem `CONFIG_MODULE_SIG`; não há exigência de assinatura.
- O módulo da ROM aparece como `[permanent]`; o nosso é descarregável
  (`CONFIG_MODULE_UNLOAD=y`).

## Validação

Feito no kernel da Fase 1 (`5.4.302-android12-5.4-exynos-ga273fe61262c`,
config idêntica à `out/.config`):

- `make modules`: `exit=0`, sem erros; `cmupmucal.ko` e `undervolt_o1s.ko`
  gerados;
- `modinfo`: vermagic correto e `depends: cmupmucal`;
- `insmod` manual: `rc=0`, `dmesg` mostra
  `undervolt: ready (cpucl0=1%, cpucl1=1%, cpucl2=1%, gpu=1%)`;
- o `cmupmucal` registra os quatro pedidos:
  `exynos_acpm_set_volt_margin: [CPUCL0] +margin -1 uV` (e CL1, CL2, G3D);
- sysfs criado com os quatro atributos em `0600`, lendo `1`;
- escrita de `2` em `cpucl0_uv_percent` gera `[CPUCL0] +margin -2 uV`;
- escrita de `101` é recusada com `EINVAL`;
- `rmmod` restaura os quatro domínios para `0` e remove o sysfs;
- recarregar reaplica os defaults;
- `service.sh` executado no contexto `u:r:ksu:s0` carrega o módulo;
- após reboot, o módulo carregou sozinho em ~14 s, os quatro offsets estavam em
  `1` e o `dmesg` registrou os quatro pedidos;
- `sys.boot_completed=1`, release esperado, root `u:r:ksu:s0` e 272 módulos
  carregados (271 da ROM + `undervolt_o1s`);
- o gerenciador reconhece o módulo com `web: true`.

### Default final

O [protocolo](../protocolo-undervolt.md) rodou os casos 0 e 1 em idle; os dois
empataram dentro da resolução do medidor (~10% em 3 h) e não houve
instabilidade. Por decisão do usuário, os casos 2 e 3 foram dispensados e o
default ficou `3 3 3 1` — 3% nos três clusters de CPU e 1% na GPU —, valor que
já vinha se mostrando estável em uso real.

Recompilado apenas o `.ko` (`make modules`, sem `Image` novo):

- `insmod` do `.ko` novo aplicou `3 3 3 1`;
- `dmesg`: `undervolt: ready (cpucl0=3%, cpucl1=3%, cpucl2=3%, gpu=1%)`;
- o `cmupmucal` registrou `[CPUCL0] +margin -3 uV`, `[CPUCL1] -3 uV`,
  `[CPUCL2] -3 uV` e `[G3D] -1 uV`;
- `modinfo` mantém o vermagic
  `5.4.302-android12-5.4-exynos-ga273fe61262c` e `depends: cmupmucal`.

**Pendente:**

- abrir a WebUI no gerenciador e conferir leitura/escrita pela interface;
- se for preciso separar diferenças de consumo menores que a resolução do
  medidor, fazer uma rodada noturna longa (8 h ou mais).

O harness que aplica os casos e coleta as métricas é local, não versionado, e
não é exigido pelo kernel. O fluxo e os critérios estão no protocolo.

## Como evoluir

- Suba um domínio por vez e no máximo `1%` por etapa.
- O teste mais sensível é idle longo e suspend: é onde o undervolt agressivo
  causa reboot aleatório.
- Se houver instabilidade, zere o domínio afetado e reinicie.
- Evite offsets em domínios de câmera/modem sem teste específico; o módulo
  cobre apenas CL0, CL1, CL2 e G3D.
- Se `insmod` falhar após uma mudança de kernel, confira primeiro vermagic e
  CRCs (`disagrees about version of symbol`).
- A interface `percent_margin` da ROM continua existindo; não use as duas ao
  mesmo tempo para o mesmo domínio sem entender que ambas enviam margem ao
  ACPM.

## Rollback

Sem reboot:

```sh
rmmod undervolt_o1s
```

O `module_exit` restaura `0` nos quatro domínios. Para remover a persistência:

```sh
rm -rf /data/adb/modules/undervolt-o1s
```

No repositório:

```sh
git revert dae25faee
git revert 5bcdcd913
git revert 0f9e42a50
```

Não há imagem de kernel nova envolvida; o aparelho continua no kernel da
Fase 1.

## Créditos

Implementação autoral. Usa apenas as APIs exportadas pelo `cmupmucal.ko` da
ROM (Samsung/LineageOS). A abordagem de expor um offset percentual por domínio
foi inspirada no Floppy `27200b8ad` (Flopster101, Floppy1280), mas nenhum código
do patch foi portado. Licença GPL-2.0.
