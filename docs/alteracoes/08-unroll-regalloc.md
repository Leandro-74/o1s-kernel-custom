# Unroll e regalloc

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `8fbae7443` |
| Arquivos | `Makefile` |
| Origem | FloppyKernel `0c0af29ab`, `7323178d6` |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

Além do LTO, o Floppy usa dois ajustes de codegen do LLVM: um limite maior de
desenrolamento de laços e o “advisor” de alocação de registradores no modo
release. Ambos afetam o código gerado sem mudar a semântica.

## Mudança

No `Makefile`, no bloco de LTO/codegen:

```make
# Codegen aggressiveness from FloppyKernel (Flopster101)
ifdef CONFIG_CC_IS_CLANG
KBUILD_CFLAGS += -mllvm -unroll-threshold=600
KBUILD_CFLAGS += -mllvm -regalloc-enable-advisor=release
LD_FLAGS_LTO_CLANG += -mllvm -regalloc-enable-advisor=release
endif
```

## Mecanismo

- `-unroll-threshold=600` permite que o LLVM desenrole laços cujo custo estimado
  fica abaixo de 600 (o padrão é bem menor). Laços pequenos e médios perdem o
  overhead de controle; em troca, o binário cresce e pode haver pressão de
  i-cache.
- `-regalloc-enable-advisor=release` ativa o modo release do advisor de alocação
  de registradores. Ele prioriza a qualidade da alocação (menos spilling) em vez
  do tempo de compilação.
- As flags são passadas tanto na compilação (`KBUILD_CFLAGS`) quanto no link
  (`LD_FLAGS_LTO_CLANG`), porque o LTO também decide alocação de registradores
  na fase de link.

Custo: tempo de build e tamanho de imagem. Não há garantia de ganho em todos os
caminhos; o efeito depende do código.

## Compatibilidade

- Não altera ABI, structs, release string nem módulos pré-compilados.
- Funciona com CFI/PAC/SCS; são transformações de codegen.
- O `ifdef CONFIG_CC_IS_CLANG` evita passar `-mllvm` para o GCC.

## Validação

- Build `#3` sem erro.
- Boot, 271 módulos e root OK.
- Nenhum `BUG:`/panic/CFI novo no dmesg.

## Como evoluir

- Se a imagem ficar grande demais ou houver suspeita de i-cache pressure, reduza
  `unroll-threshold` primeiro (por exemplo, 400) e meça.
- O ganho do `regalloc-enable-advisor` é difícil de isolar; se for comparar,
  faça builds A/B com a mesma config e meça benchmarks de CPU.
- Não coloque essas flags em `KBUILD_CFLAGS` sem `CONFIG_CC_IS_CLANG`; o GCC não
  entende `-mllvm`.

## Rollback

```sh
git revert 8fbae7443
```

## Créditos

**Flopster101** e **Floppy1280** — FloppyKernel commits `0c0af29ab` e
`7323178d6`. Ver `docs/creditos.md`.
