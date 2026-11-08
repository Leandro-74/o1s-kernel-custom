# Remover snapshot do scheduler

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `d19039923` |
| Arquivos | `kernel/sched/core.c` |
| Origem | FloppyKernel `4bc127568` |
| Risco | baixo (perda de diagnóstico Samsung) |
| Validação | build `#3`, boot, uso normal |

## Contexto

O kernel Samsung chama `dbg_snapshot_task()` dentro de `__schedule()`, a cada
troca de contexto. A função coleta estado de task/IRQ para o mecanismo de
postmortem da Samsung (ramdump). Em produção, é custo puro no caminho mais quente
do scheduler.

## Mudança

```diff
 	if (rq->clock_update_flags & RQCF_UPDATED)
 		update_rq_clock(rq);
 
 	...
 	rq_unlock_irq(rq, &rf);
-
-	dbg_snapshot_task(smp_processor_id(), rq->curr);
 	balance_callback(rq);
 }
```

## Mecanismo

A chamada acontecia depois de liberar o `rq` e antes de `balance_callback`, ou
seja, em toda preempção/context switch. Ela percorre estruturas de debug e
registra um snapshot. Removê-la elimina esse trabalho por switch.

O símbolo e a implementação de `dbg_snapshot_task()` continuam na árvore para
outros chamadores; só este ponto foi removido. Nenhuma struct muda.

## Compatibilidade

- Não altera ABI, layout de structs, release string nem módulos.
- O mecanismo de debug da Samsung perde o snapshot por context switch. Se for
  necessário investigar um reboot com ramdump, essa perda pode importar.
- `CONFIG_DEBUG_SNAPSHOT` e afins continuam compilados; só a chamada no
  `__schedule` saiu.

## Validação

- Build `#3` e boot OK.
- 271 módulos carregados.
- Nenhum `BUG:`/panic novo.
- Uso normal (root, rede, ZRAM) sem regressão.

## Como evoluir

- Se for depurar um panic/reboot e precisar do snapshot por switch, re-adicione a
  chamada temporariamente em um build de diagnóstico e meça o custo.
- Não remova a função `dbg_snapshot_task()` do resto da árvore sem entender quem
  mais a usa.
- O ganho é pequeno por switch, mas aparece em cargas com muitas trocas de
  contexto.

## Rollback

```sh
git revert d19039923
```

## Créditos

**Flopster101** — FloppyKernel `4bc127568`. Código original de debug:
**Samsung Electronics**.
