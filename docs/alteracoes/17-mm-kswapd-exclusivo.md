# kswapd com wakeup exclusivo

| Campo | Valor |
|---|---|
| Status | parcial (teste de pressão de memória pendente) |
| Commit | `d8325bf5c` |
| Arquivos | `mm/vmscan.c` |
| Origem | FloppyKernel `bbe99e5e7` |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

`kswapd_try_to_sleep()` colocava o kswapd em uma espera **não exclusiva** na
`pgdat->kswapd_wait`. Quando alguém chamava `wake_up` nessa fila, todos os
waiters podiam ser acordados e tentar reivindicar o mesmo pedido de reclaim,
disputando a flag compartilhada e varrendo as mesmas zonas.

## Mudança

```diff
-	prepare_to_wait(&pgdat->kswapd_wait, &wait, TASK_INTERRUPTIBLE);
+	/*
+	 * Exclusive wait so a single wakeup drives one thread; otherwise
+	 * every thread clears the shared request and scans the same zones.
+	 */
+	prepare_to_wait_exclusive(&pgdat->kswapd_wait, &wait,
+				  TASK_INTERRUPTIBLE);
```

A mesma troca foi feita no segundo `prepare_to_wait` do laço, depois do
`finish_wait`.

## Mecanismo

`prepare_to_wait_exclusive()` marca o waiter como exclusivo. Um `wake_up()` na
fila acorda **um** waiter exclusivo (o primeiro), em vez de todos. Isso evita
acordar threads que vão encontrar o pedido já atendido e sair, e reduz o
“thundering herd” no reclaim.

O segundo ponto é importante: o laço faz `finish_wait` e volta a dormir; sem a
troca ali, o comportamento exclusivo valeria só na primeira espera.

## Compatibilidade

- Não altera ABI, structs, release string nem módulos.
- `kswapd_wait` é interno do mm; a semântica exclusiva é a esperada para um
  consumidor único.
- Se outro caminho usar `wake_up_all()` nessa fila, todos ainda acordam; a
  mudança só afeta `wake_up()`.

## Validação

- Build `#3` e boot OK.
- 271 módulos carregados.
- ZRAM e swap funcionais.
- **Pendente:** teste sob pressão de memória (`stress-ng --vm`, alocar até
  quase o limite) e conferir `vmstat`/`/proc/vmstat` para reclaim redundante.

## Como evoluir

- Se houver stall de reclaim ou OOM precoce, compare `pgscan_*` e
  `pgsteal_*` antes/depois.
- Não troque para `prepare_to_wait_exclusive` em filas onde vários consumidores
  precisam acordar juntos; aqui é um consumidor por node.
- O patch do Floppy é pequeno e independente; se precisar rebasar, ele tende a
  aplicar limpo no `vmscan.c` 5.4.

## Rollback

```sh
git revert d8325bf5c
```

## Créditos

**Flopster101** — FloppyKernel `bbe99e5e7`. `vmscan.c`: kernel Linux (Linus
Torvalds e colaboradores), com código Samsung.
