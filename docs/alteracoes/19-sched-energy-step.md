# `energy_step` sem rwsem

| Campo | Valor |
|---|---|
| Status | parcial (térmico/carga longa pendente) |
| Commit | `ca145cc42` |
| Arquivos | `kernel/sched/ems/energy_step.c` |
| Origem | FloppyKernel `c882fbadc` |
| Risco | médio |
| Validação | build `#3`, boot, frequências |

## Contexto

O governor `energy_step` do EMS usava `policy->rwsem` em torno de
`__cpufreq_driver_target()` no `esgov_work()`. Esse lock também é usado por
sysfs, thermal e outros caminhos de frequência, então o caminho quente podia
ficar esperando. Além disso, o `esgov_update()` aplicava o rate delay e o
postpone também para **subidas** de frequência, atrasando scale-up.

## Mudança

### `esgov_work()`

- Remove `down_write(&esg_policy->policy->rwsem)` / `up_write(...)`.
- Mantém `work_lock` em volta de `__cpufreq_driver_target()`.
- Move `work_in_progress = false` para o fim, sob `update_lock`.
- Adiciona um **recheck limitado de scale-up**: se, depois do primeiro alvo,
  `target_freq` ficou maior que a frequência aplicada, aplica o novo alvo uma
  vez.

```c
mutex_lock(&esg_policy->work_lock);
__cpufreq_driver_target(esg_policy->policy, freq, CPUFREQ_RELATION_L);
mutex_unlock(&esg_policy->work_lock);

/* Bounded scale-up re-check */
raw_spin_lock_irqsave(&esg_policy->update_lock, flags);
if (esg_policy->target_freq > freq) {
	freq = esg_policy->target_freq;
	raw_spin_unlock_irqrestore(&esg_policy->update_lock, flags);
	mutex_lock(&esg_policy->work_lock);
	__cpufreq_driver_target(esg_policy->policy, freq, CPUFREQ_RELATION_L);
	mutex_unlock(&esg_policy->work_lock);
	raw_spin_lock_irqsave(&esg_policy->update_lock, flags);
}
esg_policy->work_in_progress = false;
raw_spin_unlock_irqrestore(&esg_policy->update_lock, flags);
```

### `esgov_update()`

- Remove a checagem antecipada de rate delay (`if (!rapid_scale && ...) goto out`).
- Aplica rate delay e postpone **somente em descida**:
  `if (target_freq < esg_policy->target_freq)`.
- Compara com `esg_policy->target_freq` (último alvo pendente), não com
  `policy->cur` (frequência atual do hardware).
- Atualiza `target_freq` e `last_freq_update_time` **antes** de checar
  `work_in_progress`, para que um trabalho já enfileirado pegue o alvo mais novo.

## Mecanismo

O `work_lock` já serializa as chamadas de `__cpufreq_driver_target`; o
`policy->rwsem` era redundante nesse caminho e adicionava contenção com sysfs e
thermal (o `sugov_work()` upstream também não o segura).

Separar subida de descida permite que o governor responda rápido a aumentos de
carga (sem esperar a janela de rate limit), enquanto mantém histerese para
descidas, evitando oscilação. O recheck cobre o caso em que uma subida chega
enquanto o trabalho anterior ainda está aplicando uma frequência menor.

Custo/risco: durante um trabalho em andamento, uma **descida** de alvo pode não
ser aplicada até o próximo update; o recheck só cobre subidas. Isso é
intencional para não criar uma cascata de chamadas, mas precisa ser observado em
carga sustentada.

## Compatibilidade

- Não altera ABI, structs, release string nem módulos.
- O governor continua registrado como `energy_step`; sysfs/thermal não mudam.
- `__cpufreq_driver_target` continua sendo chamado com `CPUFREQ_RELATION_L`.

## Validação

- Build `#3` e boot OK.
- Frequências observadas: little 533 MHz em idle; prime 2,6 GHz sob carga de
  boot.
- 271 módulos carregados.
- **Pendente:** teste térmico sob carga longa, benchmark de latência e
  verificação de que não há oscilação de frequência.

## Como evoluir

- Se aparecer frequência presa em baixa durante carga, investigue o recheck e o
  `target_freq` pendente antes de reintroduzir o rwsem.
- Se houver oscilação em descida, aumente a janela de rate delay em vez de
  segurar o rwsem.
- Meça com `perf`/`cpufreq-info` ou tracepoints do EMS; não confie só na
  impressão de uso.
- O patch é do Floppy e aplica limpo neste `energy_step.c`; em um rebase,
  confira se `esgov_check_rate_delay` e `esgov_postpone_freq_update` mantêm a
  assinatura.

## Rollback

```sh
git revert ca145cc42
```

## Créditos

**Flopster101** — FloppyKernel `c882fbadc`. EMS: **Samsung Electronics**.
