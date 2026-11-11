# Undervolt por módulo de kernel

| Campo | Valor |
|---|---|
| Status | validado (build, runtime, boot e prova física nos PMICs); default `3 3 3 1`, teto de 10% |
| Commit(s) | `0f9e42a50`, `5bcdcd913`, `dae25faee`, `f519b1140`, `25699e292`, `325b10153`, `a68ae6335`, `2de81995f` (branches `undervolt-module` e `undervolt-ui`, mescladas em `resukisu`) |
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
aceita é `0..10` — acima disso a escrita é recusada com `EINVAL` antes de
chegar ao ACPM.

O módulo do KernelSU em `ksu-modules/undervolt-o1s/` contém:

- `undervolt_o1s.ko` (copiado do build, não versionado);
- `service.sh` que espera o `cmupmucal` e faz `insmod`;
- `module.prop`;
- `webroot/index.html` com o painel WebUI;
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

No `store`, o valor é validado em `0..10` e convertido para margem negativa. No
`module_exit`, os quatro domínios recebem `0` antes de remover o sysfs.

O `service.sh` roda no `late_start` do KernelSU, espera
`/sys/kernel/percent_margin` (sinal de que o `cmupmucal` carregou), confere o
`lsmod` e faz `insmod`. O `.ko` fica no diretório do módulo com contexto
`u:object_r:ksu_file:s0`.

### Limite e painel

O teto de `10%` é a única proteção automática e fica no kernel, não na
interface: `echo 11` em qualquer domínio retorna `EINVAL` e o rail não é tocado.
Isso evita que um valor absurdo digitado no terminal chegue ao PMIC. O controle
positivo mostrou que `10%` em `cpucl1` a 1,768 GHz já reinicia o aparelho por
watchdog; o teto impede valores acima disso, mas `10%` continua agressivo e
exige teste.

Os offsets são voláteis por decisão: o `service.sh` só faz `insmod` e o
`module_init` aplica o Kconfig. Um reboot sempre volta para `3 3 3 1`, então não
existe bootloop por valor salvo. Para persistir no futuro seria preciso aplicar
o candidato, marcar o boot e reverter para o último valor bom se o aparelho
reiniciar antes da confirmação.

O painel em `webroot/index.html` segue o tema do sistema (claro/escuro) e mostra,
por domínio, um slider de `0` a `10`, o valor aplicado, um ponto quando há
alteração pendente e a tensão atual do rail em mV. A tensão sai dos mesmos
registradores usados na prova física:

| Domínio | PMIC | Registrador |
|---|---|---|
| cpucl0 | s2mps24 BUCK2S OUT2 | `0x1f` |
| cpucl1 | s2mps24 BUCK3S OUT2 | `0x23` |
| cpucl2 | s2mps24 BUCK4S OUT2 | `0x27` |
| gpu | s2mps23 BUCK1 | `0x1e` |

`mV = 300 + 6,25 × valor`. É a tensão do OPP em uso, não a do OPP máximo, então
o número muda com a frequência. Valores acima de `5%` pedem confirmação na
interface; o botão *Restaurar padrão* volta para `3 3 3 1`.

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
- O sysfs é `0600` (root apenas) e o valor é limitado a `0..10` no kernel.
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
- escrita de `101` (e de `11`, `20` ou `-1`) é recusada com `EINVAL`, sem tocar
  no rail;
- `rmmod` restaura os quatro domínios para `0` e remove o sysfs;
- recarregar reaplica os defaults;
- `service.sh` executado no contexto `u:r:ksu:s0` carrega o módulo;
- após reboot, o módulo carregou sozinho em ~14 s, os quatro offsets estavam em
  `1` e o `dmesg` registrou os quatro pedidos;
- `sys.boot_completed=1`, release esperado, root `u:r:ksu:s0` e 272 módulos
  carregados (271 da ROM + `undervolt_o1s`);
- o gerenciador reconhece o módulo com `web: true`.

### Limite de 10% e painel novo (versão 2.1)

- `make modules`: `exit=0`, 271 módulos, vermagic inalterado;
- `rmmod` + `insmod` do `.ko` novo reaplicou `3 3 3 1`;
- `11`, `20`, `101` e `-1` nos quatro domínios retornam `EINVAL`, os valores
  continuam `3 3 3 1` e o `dmesg` não registra margem nova;
- o comando de leitura dos rails usado pelo painel devolve os quatro valores em
  decimal no aparelho (ex.: `70`, `52`, `61`, `36`);
- o painel passou por 28 verificações em navegador com dados simulados: estado
  inicial, ponto de pendência, faixas de cor, gravação, confirmação acima de
  `5%`, cancelamento e reset.

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

### Prova física

O sysfs só mostra o que o módulo pediu. Para provar que a margem chega ao
hardware, foi lida a tensão que o ACPM **programou** nos reguladores, direto dos
registradores dos PMICs (`s2mps23_read` e `s2mps24_read`, via proxy do ACPM).

Os rails foram identificados no aparelho alternando um domínio por vez:

| Domínio | Rail | Registrador | Mínimo | Passo |
|---|---|---|---|---|
| CPUCL0 (LITTLE) | s2mps24 BUCK2S OUT2 | `0x011f` | 300 mV | 6,25 mV |
| CPUCL1 (A76) | s2mps24 BUCK3S OUT2 | `0x0123` | 300 mV | 6,25 mV |
| CPUCL2 (X1) | s2mps24 BUCK4S OUT2 | `0x0127` | 300 mV | 6,25 mV |
| G3D (GPU) | s2mps23 BUCK1 | `0x011e` | 300 mV | 6,25 mV |

Os três clusters de CPU ficam no **sub-PMIC s2mps24**; a GPU, no s2mps23.
`mV = 300 + 0,00625 × valor`.

Cada cluster foi preso na frequência mais alta com carga, e a margem do domínio
alternada entre `0` e o default, quatro vezes. Resultado (4/4 pares iguais em
cada linha):

| Domínio | Frequência | 0% | default | Queda |
|---|---|---|---|---|
| cpucl0 | 2.210 MHz | 987,50 mV (`0x6e`) | 968,75 mV (`0x6b`) | 18,75 mV (3 passos) |
| cpucl1 | 1.768 MHz | 737,50 mV (`0x46`) | 718,75 mV (`0x43`) | 18,75 mV (3 passos) |
| cpucl2 | 2.496 MHz | 925,00 mV (`0x64`) | 900,00 mV (`0x60`) | 25,00 mV (4 passos) |
| gpu | 858 MHz travado | 762,50 mV (`0x4a`) | 756,25 mV (`0x49`) | 6,25 mV (1 passo) |

A queda é imediata: não é preciso transição de DVFS depois de escrever a margem,
o ACPM reprograma o rail na hora. O número de passos é o truncamento de 3% (ou
1% na GPU) sobre a tensão do OPP, arredondado para baixo no passo de 6,25 mV —
por isso 3 passos em CL0/CL1 e 4 em CL2, cujo OPP é mais alto.

### Controle positivo: o que acontece se exagerar

O mesmo teste com `10%` em `cpucl1` a 1,768 GHz **reiniciou o aparelho** cerca de
0,7 s depois da escrita, sem panic no `pstore`:

- `/proc/reset_reason` = `DPON` (`RR_D` = 3, watchdog);
- `reset_summary`: `WATCHDOG RESET`, `WDGC` desconhecido;
- `pstore` termina logo depois de
  `[CPUCL1] +margin -10 uV` e de uma leitura `s2mps23_read_store: reg(0x0129)`;
- `persist.sys.boot.reason.history` registra `reboot` (sem `,shell`), ou seja,
  não foi reboot por comando, e o usuário confirmou que não reiniciou o aparelho
  manualmente.

É a demonstração inversa: um valor que só existisse em arquivo não derrubaria o
aparelho. Por isso a margem de teste fica limitada ao default, e o harness não
sobe acima dele.

### Suspend com o default

Com `3 3 3 1`, o aparelho ficou ~15 min em suspend profundo (buraco de 847 s no
coletor, `btime` inalterado), sem reboot, panic ou incidente, e os offsets
continuaram `3 3 3 1` ao acordar.

O harness que aplica os casos e coleta as métricas é local, não versionado, e
não é exigido pelo kernel. O fluxo e os critérios estão no protocolo.

## Como evoluir

- Suba um domínio por vez e no máximo `1%` por etapa; o teto de `10%` é fixo no
  código (`UV_MAX_PERCENT`) e exige recompilar o `.ko` para mudar.
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
