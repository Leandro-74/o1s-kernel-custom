# ThinLTO e inlining

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `fb49f9c24` |
| Arquivos | `Makefile` |
| Origem | FloppyKernel `0c0af29ab`, `92bc2ca32`, `7323178d6` |
| Risco | baixo |
| Validação | build `#3`, boot, CFI ativo |

## Contexto

O kernel já usava ThinLTO (`-flto=thin -fsplit-lto-unit`) por causa do build
stock. O Floppy ajusta os limites do LTO para importar e inlinar mais funções
entre unidades de tradução, o que reduz chamadas e melhora o código final.

## Mudança

No bloco `CONFIG_LTO_CLANG`/`CONFIG_THINLTO` do `Makefile`:

```make
ifdef CONFIG_THINLTO
CC_FLAGS_LTO_CLANG := -flto=thin $(call cc-option, -fsplit-lto-unit)

# LLVM tunings from FloppyKernel (Floppy1280, Flopster101)
LD_FLAGS_LTO_CLANG += -mllvm -import-hot-multiplier=2
LD_FLAGS_LTO_CLANG += -mllvm -inline-threshold=300
LD_FLAGS_LTO_CLANG += -mllvm -import-instr-limit=30
else
CC_FLAGS_LTO_CLANG := -flto
endif
CC_FLAGS_LTO_CLANG += -fvisibility=default

ifndef CONFIG_THINLTO
# Limit inlining across translation units to reduce binary size
LD_FLAGS_LTO_CLANG += -mllvm -import-instr-limit=5
endif
```

## Mecanismo

- `-import-hot-multiplier=2` aumenta o peso dado a funções quentes na hora de
  decidir o que importar de outra unidade no ThinLTO.
- `-inline-threshold=300` sobe o limite de custo para inlining (o padrão do LLVM
  é menor). Mais funções pequenas/médias entram no corpo do chamador.
- `-import-instr-limit=30` permite importar funções maiores entre unidades. Antes,
  o limite de 5 era aplicado inclusive no ThinLTO; agora ele vale só para LTO
  completo.
- `-fsplit-lto-unit` foi **mantido**. O clang rejeita
  `-fno-split-lto-unit` junto com `-fsanitize=cfi`; como CFI é obrigatório para
  os módulos da ROM, não há como remover.
- As flags entram em `LD_FLAGS_LTO_CLANG`, que é aplicado a `KBUILD_LDFLAGS` e
  `KBUILD_LDFLAGS_MODULE`. Isso importa para os módulos compilados na árvore.

Custo: mais inlining significa mais código e mais tempo de link. O build `#3`
ficou com `Image` de 34.148.864 bytes (o stock anterior tinha 30.870.016).

## Compatibilidade

- Não altera ABI, structs nem release string.
- CFI continua habilitado; `-fsplit-lto-unit` é pré-requisito dele.
- Os 271 módulos pré-compilados da ROM não são relinkados por essas flags; só os
  módulos in-tree.
- O `cc-option` em `-fsplit-lto-unit` mantém compatibilidade com toolchains que
  não conhecem a flag.

## Validação

- Build `#3` sem erro de LTO/CFI.
- `CONFIG_LTO_CLANG=y`, `CONFIG_THINLTO=y`, `CONFIG_CFI_CLANG=y` no `out/.config`.
- Boot e 271 módulos OK.
- Nenhuma falha de CFI no dmesg.

## Como evoluir

- Ajuste **uma** flag por vez e meça boot, latência de abertura de apps e tamanho
  de imagem. `inline-threshold` e `import-instr-limit` são os que mais inflam o
  binário.
- Não tente `-fno-split-lto-unit` enquanto CFI estiver ligado.
- Se o link passar a demorar demais, reduza `import-instr-limit` antes de mexer
  no `inline-threshold`.
- Mantenha o `ifndef CONFIG_THINLTO` para não aplicar o limite de 5 no modo
  ThinLTO.

## Rollback

```sh
git revert fb49f9c24
```

Volta aos limites padrão do LTO. O build continua válido; muda desempenho e
tamanho.

## Créditos

**Flopster101** e **Floppy1280** — FloppyKernel commits `0c0af29ab`, `92bc2ca32`
e `7323178d6`. Ver `docs/creditos.md`.
