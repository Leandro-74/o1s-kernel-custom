# Rede: BBR como congestion control padrão

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `a6f15406f` |
| Arquivos | `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | config de BBR do FloppyKernel |
| Risco | baixo |
| Validação | `sysctl` no aparelho |

## Contexto

O kernel já compilava o BBR (`CONFIG_TCP_CONG_BBR=y`), mas o padrão continuava
sendo o CUBIC. O Floppy usa BBR como padrão para melhorar o comportamento em
redes com perda e latência variável, típicas de mobile.

## Mudança

```kconfig
# Rede
CONFIG_DEFAULT_BBR=y
```

## Mecanismo

`CONFIG_DEFAULT_BBR` seleciona o BBR no `choice` de congestion control padrão do
`net/ipv4/Kconfig`. Com isso, `net.ipv4.tcp_congestion_control` nasce como `bbr`,
mas continua ajustável em runtime por sysctl. O BBR modela banda e RTT em vez de
reagir só a perda, o que costuma render melhor throughput em Wi-Fi/celular.

Custo: o BBR pode ser menos “justo” que o CUBIC contra fluxos loss-based e tem
comportamento diferente em links muito assimétricos. Em mobile, o ganho costuma
compensar.

## Compatibilidade

- É só uma config; não altera ABI, structs nem release string.
- `CONFIG_TCP_CONG_BBR=y` precisa estar ligado; caso contrário a opção não
  aparece.
- Scripts de init/ROM podem sobrescrever o sysctl; o padrão do kernel não impede
  isso.

## Validação

- `sysctl net.ipv4.tcp_congestion_control` → `bbr`.
- Boot e 271 módulos OK.
- Não foi feito benchmark de throughput comparativo.

## Como evoluir

- Para voltar ao CUBIC sem rebuild:
  `sysctl -w net.ipv4.tcp_congestion_control=cubic`.
- Se o aparelho apresentar problemas em redes específicas, teste CUBIC antes de
  atribuir a falha a outro patch de rede.
- Não fixe o algoritmo em código de driver; mantenha o padrão via Kconfig e
  deixe o runtime ajustar.

## Rollback

```sh
git revert a6f15406f
```

## Créditos

**Flopster101** (config do FloppyKernel). BBR: **Neal Cardwell**, **Yuchung
Cheng**, **Van Jacobson** e **Soheil Hassas Yeganeh** (Google).
