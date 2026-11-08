# Unroll nas libs de compressão

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `e07c775c7` |
| Arquivos | `lib/lz4/Makefile`, `lib/lzo/Makefile`, `lib/zstd/Makefile` |
| Origem | FloppyKernel `94ded34ca` |
| Risco | baixo |
| Validação | build `#3`, ZRAM ativo com lzo-rle |

## Contexto

Compressão e descompressão aparecem no caminho de memória (ZRAM), no initramfs e
em outros consumidores das libs `lz4`, `lzo` e `zstd`. O Floppy compila essas
libs com desenrolamento de laços para reduzir o custo por bloco.

## Mudança

```make
# lib/lz4/Makefile
ccflags-y += -O3 -funroll-loops

# lib/lzo/Makefile
ccflags-y += -O3 -funroll-loops

# lib/zstd/Makefile
ccflags-y += -O3 -funroll-loops
```

Antes, `lz4` e `zstd` já tinham `-O3`; `lzo` não tinha otimização explícita. O
commit adiciona `-funroll-loops` nos três e `-O3` no `lzo`.

## Mecanismo

- `-funroll-loops` duplica o corpo de laços com contagem conhecida ou pequena,
  eliminando branches e melhorando o pipeline.
- `-O3` liga otimizações mais agressivas (incluindo vetorização quando o alvo
  permite).
- O efeito é maior na descompressão, que é o caminho mais frequente no ZRAM.

Custo: código maior nas libs. O ganho depende do algoritmo e do bloco; não foi
medido com benchmark isolado.

## Compatibilidade

- As libs mantêm a mesma API e o mesmo formato de dados.
- Não altera ABI, structs, release string nem módulos.
- As flags valem só para os objetos dessas pastas.

## Validação

- Build `#3` sem erro.
- ZRAM ativo (~2,1 GB, algoritmo `lzo-rle`) e swap funcional.
- Sem `BUG:`/panic no uso normal.

## Como evoluir

- Para medir, compare tempo de `zramctl`/operações de swap ou use
  `perf stat` durante compressão/descompressão.
- Se o tamanho da imagem for crítico, o primeiro candidato a reverter é o `-O3`
  do `lzo`; o `-funroll-loops` costuma dar ganho maior no caminho quente.
- Não adicione `-funroll-loops` em libs sem benchmark: o custo de i-cache pode
  superar o ganho.

## Rollback

```sh
git revert e07c775c7
```

## Créditos

**Flopster101** — FloppyKernel commit `94ded34ca`. As libs são de **Yann Collet**
(LZ4), **Markus Oberhumer** (LZO) e **Meta/Facebook + Yann Collet** (Zstandard).
Ver `docs/creditos.md`.
