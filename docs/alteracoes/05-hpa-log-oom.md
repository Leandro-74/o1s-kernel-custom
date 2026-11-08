# Formato do log de OOM no HPA

| Campo | Valor |
|---|---|
| Status | implementado |
| Commit | `deb565caa` |
| Arquivos | `mm/hpa.c` |
| Origem | correção local |
| Risco | baixo |
| Validação | build |

## Contexto

O `hpa_killer()` do Samsung HPA (Hyper Page Allocation) imprime o ajuste do
processo escolhido para morrer quando falta memória. O log usava `%hd`, mas o
valor passado (`selected_adj`) é `int`.

## Mudança

```diff
-	pr_info("Killing '%s' (%d), adj %hd to free %lukB\n",
+	pr_info("Killing '%s' (%d), adj %d to free %lukB\n",
 		selected->comm, task_pid_nr(selected), selected_adj,
 		selected_tasksize * (PAGE_SIZE / SZ_1K));
```

## Mecanismo

Em chamadas variádicas, `%hd` espera um `int` promovido e o lê como `short`. O
valor é truncado para 16 bits na impressão. Para ajustes pequenos (a faixa usada
pelo OOM killer) o resultado coincide, mas a formatação fica incorreta para
valores fora da faixa e o compilador pode emitir warning com `-Wformat`.

A correção não muda a decisão do OOM killer nem o processo escolhido; só o texto
do log.

## Compatibilidade

- Não altera structs, ABI, release string nem módulos.
- `hpa.c` é específico da Samsung; a correção não afeta outros alvos do mesmo
  jeito que um patch de core afetaria.

## Validação

- Build `Image` sem warning de formato nesse ponto.
- O log só aparece sob pressão de memória; não foi provocado OOM em teste.

## Como evoluir

- Se for adicionar campos ao log, confira o tipo de cada argumento (`int`,
  `unsigned int`, `pid_t`, `size_t`) antes de escolher o especificador.
- `selected_tasksize * (PAGE_SIZE / SZ_1K)` é `unsigned long`; `%lukB` está
  correto para esse tamanho.
- Não use esse arquivo como ponto de partida para mudanças de política de OOM;
  ele é só o matador do HPA.

## Rollback

```sh
git revert deb565caa
```

## Créditos

Correção local sobre código do **Samsung Electronics** (HPA).
