# Config do BakaSU e decisão sobre kprobes

| Campo | Valor |
|---|---|
| Status | validado |
| Commits | `f8c662c02`, `bda28371e` |
| Arquivos | `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | integração BakaSU |
| Risco | baixo |
| Validação | build `#2` e `#3`, root no aparelho |

## Contexto

O BakaSU precisa ser compilado dentro do kernel (`=y`) e usar o modo de hooks
manuais. O plano original também recomendava desligar `CONFIG_KPROBES` para
evitar o safe mode do KernelSU em kernels antigos.

## Mudança

No fragmento `o1s.config`:

```kconfig
# BakaSU
CONFIG_KSU=y
CONFIG_KSU_MANUAL_HOOK=y
```

O commit `f8c662c02` adicionou também `# CONFIG_KPROBES is not set`. O commit
`bda28371e` removeu essa linha, mantendo os kprobes habilitados. A config efetiva
do build (`out/.config`) ficou com:

```
CONFIG_KSU=y
CONFIG_KSU_MANUAL_HOOK=y
CONFIG_KPROBES=y
```

## Mecanismo

- `CONFIG_KSU=y` compila o driver embutido. `KSU_MANUAL_HOOK` depende de
  `KSU != m`, então não é possível usar `=m` neste modo.
- `CONFIG_KSU_MANUAL_HOOK=y` troca o redirecionamento por tracepoint pelos hooks
  manuais dos documentos 03.
- `CONFIG_KPROBES=y` não é necessário para os hooks manuais, mas é usado por
  ferramentas de tracing/perf e por outros módulos. O kernel bootou e o root
  funcionou com ele ligado; por isso a decisão final foi mantê-lo.

O aviso do plano sobre safe mode continua válido como sintoma: se o aparelho
entrar em safe mode ou o root ficar instável, a primeira tentativa de diagnóstico
é desligar `CONFIG_KPROBES` e reconstruir.

## Compatibilidade

- `CONFIG_KSU=y` e `KSU_MANUAL_HOOK=y` não alteram ABI nem o release string.
- O gerenciador e o userspace precisam casar com o driver `35257`.
- `CONFIG_KPROBES` é uma opção do kernel; desligá-la pode afetar ferramentas que
  dependem de kprobes, então a mudança não é “de graça”.

## Validação

- `grep -E '^CONFIG_(KSU|KSU_MANUAL_HOOK|KPROBES)=' out/.config` mostra os três
  símbolos como `y`.
- Driver no aparelho: `35257`, `v4.2.0-rc3-516d3e87`.
- `su -c id` funciona; `ksud` roda `post-fs-data` e `boot-completed`.

## Como evoluir

- Se for atualizar o BakaSU, confira se `KSU_MANUAL_HOOK` continua existindo e se
  novos símbolos são necessários (por exemplo, `KSU_SUSFS`).
- Mantenha o fragmento `o1s.config` e o `out/.config` em sincronia. O build
  manual usa `out/.config`; o fragmento é o registro versionado.
- Não habilite `CONFIG_KSU_DEBUG` só para liberar `adb shell`: isso liga o modo
  debug. A liberação do `shell` é feita pelo gerenciador, em SuperUsuário.
- Se o aparelho entrar em safe mode, desligue `KPROBES` primeiro e documente o
  resultado antes de mexer em outra coisa.

## Rollback

```sh
git revert bda28371e f8c662c02
```

Reverter `f8c662c02` remove o BakaSU da config; reverter `bda28371e` volta a
desligar os kprobes (na prática, os dois commits se anulam parcialmente). O
rollback completo do root inclui os documentos 01 a 03.

## Créditos

Integração: **KernelSU** e **BakaSU**. A recomendação de desligar kprobes vem da
documentação do KernelSU para kernels antigos.
