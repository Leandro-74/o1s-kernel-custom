# Rede: TTL para tethering

| Campo | Valor |
|---|---|
| Status | parcial (sem teste funcional de tethering) |
| Commit | `89742f468` |
| Arquivos | `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | config do FloppyKernel |
| Risco | baixo |
| Validação | build `#3` |

## Contexto

O alvo `TTL` do iptables permite ajustar o TTL dos pacotes encaminhados. É usado
em tethering para evitar que a operadora detecte o compartilhamento e derrube a
conexão. A opção existia no Kconfig, mas estava desligada na config stock.

## Mudança

```kconfig
CONFIG_IP_NF_TARGET_TTL=y
```

O Kconfig seleciona automaticamente:

```kconfig
select NETFILTER_XT_TARGET_HL
```

## Mecanismo

`IP_NF_TARGET_TTL` registra o alvo `-j TTL --ttl-set/--ttl-dec/--ttl-inc` na
tabela `mangle`. Ele depende de `IP_NF_MANGLE` e de `NETFILTER_ADVANCED`; o
`select` garante o módulo de match de Hop Limit, que é a peça de baixo nível.

Sem uma regra de iptables usando `-j TTL`, a opção não tem efeito em runtime.
Ela só disponibiliza a funcionalidade.

## Compatibilidade

- É uma config de netfilter; não altera ABI, structs nem release string.
- O alvo fica built-in (`=y`), então não depende de carregar módulo em
  `vendor_dlkm`.
- Não interfere no firewall existente se nenhuma regra usar o alvo.

## Validação

- Build `#3` com `CONFIG_IP_NF_TARGET_TTL=y`.
- `CONFIG_NETFILTER_XT_TARGET_HL=y` presente no `out/.config`.
- **Pendente:** criar uma regra de teste e confirmar o TTL em pacotes
  encaminhados (hotspot/USB tethering).

## Como evoluir

- Para testar:
  `iptables -t mangle -A POSTROUTING -j TTL --ttl-set 65` e confira com
  `tcpdump` no lado do cliente.
- Não confunda com `IP_NF_MANGLE`; os dois precisam estar ligados.
- Se a ROM usar `nftables`, o alvo equivalente precisa existir no userspace; a
  opção do kernel só cobre o caminho `iptables`.

## Rollback

```sh
git revert 89742f468
```

## Créditos

**Flopster101** (config do FloppyKernel). Netfilter: kernel Linux e AOSP.
