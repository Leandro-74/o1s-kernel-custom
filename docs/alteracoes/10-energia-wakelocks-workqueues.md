# Energia: wakelocks e workqueues

| Campo | Valor |
|---|---|
| Status | parcial (drain em idle pendente) |
| Commit | `d1e26a1ae` |
| Arquivos | `arch/arm64/configs/samsung/o1s.config`, `out/.config` |
| Origem | FloppyKernel `22970adac` + configs de energia |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

Duas opções de energia estavam desligadas na config stock:

- `CONFIG_PM_WAKELOCKS_GC`: coleta wakelocks de userspace que não estão mais em
  uso.
- `CONFIG_WQ_POWER_EFFICIENT_DEFAULT`: faz workqueues não pinadas preferirem
  CPUs economicamente melhores.

## Mudança

```kconfig
# Energia
CONFIG_PM_WAKELOCKS_GC=y
CONFIG_WQ_POWER_EFFICIENT_DEFAULT=y
```

## Mecanismo

- `PM_WAKELOCKS_GC` varre a lista de wakeup sources de userspace e remove as que
  estão inativas. Isso evita que entradas antigas mantenham estatísticas e
  estruturas vivas, e reduz a chance de um wakelock “fantasma” aparecer em
  `/sys/kernel/debug/wakeup_sources`. Não bloqueia wakelocks em uso.
- `WQ_POWER_EFFICIENT_DEFAULT` muda o padrão de workqueues não pinadas: em vez de
  preferir a CPU local (melhor cache locality), o scheduler escolhe uma CPU
  economicamente eficiente. Custo possível: latência um pouco maior em cargas que
  dependem de workqueues não pinadas.

O Kconfig de `PM_WAKELOCKS_GC` tem `default y` nesta árvore, mas a config stock
do aparelho o deixava desligado; o commit torna o valor explícito.

## Compatibilidade

- São opções de config; não alteram ABI, structs nem release string.
- `PM_WAKELOCKS_GC` depende de `PM_WAKELOCKS`.
- `WQ_POWER_EFFICIENT_DEFAULT` depende de `PM`.

## Validação

- Build `#3` e boot OK.
- 271 módulos carregados.
- Nenhum WARNING novo atribuído a essas opções.
- **Pendente:** drain de 8 h em idle comparado ao build anterior; suspend/resume
  de longa duração.

## Como evoluir

- Se aparecer regressão de latência em cargas específicas, desligue
  `WQ_POWER_EFFICIENT_DEFAULT` primeiro; é a opção com custo de desempenho mais
  direto.
- Para investigar wakelocks, use
  `/sys/kernel/debug/wakeup_sources` e `dumpsys power`; o GC não substitui a
  análise.
- Meça o drain com o aparelho fora do USB e em rede estável, senão o teste não
  vale.

## Rollback

```sh
git revert d1e26a1ae
```

Volta ao comportamento stock de energia.

## Créditos

**Flopster101** — FloppyKernel `22970adac` e configs de energia. As opções são do
**Android Open Source Project (AOSP)** e do kernel Linux.
