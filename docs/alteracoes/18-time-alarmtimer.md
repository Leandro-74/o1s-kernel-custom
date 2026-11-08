# Wakelock do alarmtimer

| Campo | Valor |
|---|---|
| Status | parcial (drain em idle pendente) |
| Commit | `895a6e39a` |
| Arquivos | `kernel/time/alarmtimer.c` |
| Origem | FloppyKernel `53cab1dcb` (celtare21, Pranav Vashi) |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

No suspend, o alarmtimer segura um wakeup event por 2 s quando o próximo alarme
está a menos de 2 s, e por 1 s quando o `rtc_timer_start` falha. Esse tempo
impede o aparelho de suspender de novo imediatamente e mantém o sistema
acordado sem necessidade.

## Mudança

```diff
 	if (ktime_to_ns(min) < 2 * NSEC_PER_SEC) {
-		__pm_wakeup_event(ws, 2 * MSEC_PER_SEC);
+		__pm_wakeup_event(ws, MSEC_PER_SEC / 2);
 		return -EBUSY;
 	}
@@
 	ret = rtc_timer_start(rtc, &rtctimer, now, 0);
 	if (ret < 0)
-		__pm_wakeup_event(ws, MSEC_PER_SEC);
+		__pm_wakeup_event(ws, MSEC_PER_SEC / 2);
 	return ret;
```

Os dois prazos passam a 500 ms.

## Mecanismo

`__pm_wakeup_event()` reporta um evento de wakeup com duração. Enquanto o evento
está ativo, o `autosleep`/suspend não considera o sistema ocioso. Reduzir de 2 s
para 500 ms encurta a janela em que o aparelho fica impedido de suspender quando
o alarme está muito próximo.

O caso de falha do RTC também encurta a janela de retry: em vez de 1 s, tenta de
novo em 500 ms. Se o RTC estiver realmente quebrado, o custo é mais tentativas de
suspend, não um loop infinito.

## Compatibilidade

- Não altera ABI, structs nem release string.
- O alarmtimer continua funcional; só o prazo do wakeup event muda.
- Pode afetar o tempo de retomada de alarmes muito próximos do suspend.

## Validação

- Build `#3` e boot OK.
- 271 módulos carregados.
- **Pendente:** drain em idle de 8 h e contagem de wakeups
  (`/sys/kernel/debug/wakeup_sources`, `dumpsys alarm`).

## Como evoluir

- Se o drain não melhorar, meça antes de mexer de novo; o alarmtimer pode não ser
  o gargalo.
- Se aparecer atraso em alarmes de curto prazo, considere 1 s em vez de 500 ms.
- Não zere o wakeup event: ele existe para evitar suspend prematuro durante o
  reprograma do RTC.

## Rollback

```sh
git revert 895a6e39a
```

## Créditos

**celtare21** (patch original) e **Pranav Vashi** (sign-off), via FloppyKernel
`53cab1dcb`. Ver `docs/creditos.md`.
