# Rede: CAKE como qdisc padrão

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `f344e290b` |
| Arquivos | `net/sched/Kconfig`, `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | FloppyKernel + backport upstream |
| Risco | baixo |
| Validação | `tc qdisc` em `wlan0` |

## Contexto

O CAKE (`sch_cake`) já era compilado, mas não havia como selecioná-lo como qdisc
padrão: o `choice` de `net/sched/Kconfig` não tinha a opção `DEFAULT_CAKE`. O
Floppy inclui essa opção; aqui ela foi adicionada como backport do upstream.

## Mudança

`net/sched/Kconfig`, dentro do `choice` de qdisc padrão:

```kconfig
# Backport of the upstream option, also used by FloppyKernel.
config DEFAULT_CAKE
	bool "Common Applications Kept Enhanced (CAKE)" if NET_SCH_CAKE
```

E no `DEFAULT_NET_SCH`:

```kconfig
default "cake" if DEFAULT_CAKE
```

Config:

```kconfig
CONFIG_NET_SCH_CAKE=y
CONFIG_NET_SCH_DEFAULT=y
CONFIG_DEFAULT_CAKE=y
```

## Mecanismo

Sem `CONFIG_NET_SCH_DEFAULT`, o kernel usa `pfifo_fast` fixo. Com
`NET_SCH_DEFAULT=y` e `DEFAULT_CAKE=y`, o `DEFAULT_NET_SCH` passa a ser `cake`
para interfaces que não recebem uma qdisc explícita.

O CAKE combina shaping, AQM (CoDel/Blue) e escalonamento por fluxo. Em conexões
de upload limitado, ele reduz bufferbloat e melhora latência percebida. Custo:
mais CPU por pacote que `fq_codel`/`pfifo_fast`, mas baixo para o Exynos 2100.

## Compatibilidade

- A opção `DEFAULT_CAKE` não existia nesta árvore; o backport só adiciona o
  símbolo ao choice, sem mudar structs ou ABI.
- Se `NET_SCH_CAKE` não estiver ligado, a opção fica invisível e o choice cai no
  padrão anterior.
- Interfaces gerenciadas por scripts da ROM podem trocar a qdisc em runtime; o
  padrão do kernel não as impede.

## Validação

- `tc qdisc show dev wlan0` → `qdisc cake ...`.
- Boot e 271 módulos OK.
- Não foi medido bufferbloat/latência sob carga.

## Como evoluir

- Se algum link piorar, troque a qdisc em runtime antes de rebuildar:
  `tc qdisc replace dev wlan0 root fq_codel`.
- Para mudar o padrão, edite o `choice` e o `DEFAULT_NET_SCH` juntos; não basta
  ligar `CONFIG_DEFAULT_CAKE` sem a entrada no Kconfig.
- O CAKE pode ser integrado ao BBR; os dois atuam em camadas diferentes
  (transporte e fila).

## Rollback

```sh
git revert f344e290b
```

Volta ao qdisc padrão anterior (`pfifo_fast` ou o que o choice selecionar).

## Créditos

**Flopster101** (Kconfig do FloppyKernel) e autores upstream do CAKE, em especial
**Toke Høiland-Jørgensen**. Ver `docs/creditos.md`.
