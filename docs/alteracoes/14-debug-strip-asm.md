# Strip de símbolos do assembler

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `08e67190a` |
| Arquivos | `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | config do FloppyKernel |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

O linker mantém símbolos locais gerados pelo assembler (`.Lxxx`). Eles não são
necessários em produção, aumentam a tabela de símbolos e poluem a saída de
ferramentas como `get_wchan()`.

## Mudança

```kconfig
# Debug
CONFIG_STRIP_ASM_SYMS=y
```

## Mecanismo

`STRIP_ASM_SYMS` (em `lib/Kconfig.debug`) faz o link remover símbolos internos
gerados pelo assembler. O kernel continua com os símbolos globais/funções
necessários para `kallsyms`, módulos e stack traces; só os rótulos locais somem.

Custo: depuração de baixo nível fica um pouco mais difícil, porque labels de
assembler não aparecem mais. Em produção, o ganho é tamanho e limpeza.

## Compatibilidade

- Não altera ABI, structs, release string nem módulos.
- `kallsyms` continua funcional para funções e símbolos exportados.
- Se precisar depurar assembler, desligue a opção em um build de diagnóstico.

## Validação

- Build `#3` e boot OK.
- 271 módulos carregados.
- Nenhum `unknown symbol`.

## Como evoluir

- Em builds de debug, mantenha `CONFIG_STRIP_ASM_SYMS=n` para facilitar
  `objdump`/`gdb`.
- Não confunda com `CONFIG_DEBUG_INFO`; o strip não remove DWARF de um build com
  debug info, ele remove símbolos de assembler no link.

## Rollback

```sh
git revert 08e67190a
```

## Créditos

**Flopster101** (config do FloppyKernel). Opção do kernel Linux.
