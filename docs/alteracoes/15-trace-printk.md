# Desativar `trace_printk`

| Campo | Valor |
|---|---|
| Status | validado (aviso de módulo vendor permanece) |
| Commit | `37ee11b45` |
| Arquivos | `include/linux/kernel.h`, `kernel/trace/Kconfig`, `arch/arm64/configs/samsung/o1s.config`, `drivers/video/fbdev/exynos/dpu30/decon_core.c` |
| Origem | FloppyKernel `ca6448e4c` + `fa1921857` |
| Risco | baixo |
| Validação | build `#3`, boot |

## Contexto

`trace_printk()` liga funções de tracing e aloca buffer no ring buffer do
ftrace. Em produção isso custa memória e gera avisos de boot. O kernel stock
tinha chamadas no `decon_core.c` e em outros pontos.

## Mudança

`kernel/trace/Kconfig`:

```kconfig
if TRACING

config DISABLE_TRACE_PRINTK
	bool "Force disable trace_printk() usage"
	default y
	help
	  ...substitui trace_printk() por pr_debug()...
```

`include/linux/kernel.h`:

```c
#ifdef CONFIG_DISABLE_TRACE_PRINTK
#define trace_printk pr_debug
#else
#define trace_printk(fmt, ...) ...
#endif
```

Config:

```kconfig
CONFIG_DISABLE_TRACE_PRINTK=y
```

Correção no `decon_core.c`:

```diff
-	trace_printk(buf);
+	trace_printk("%s", buf);
```

## Mecanismo

Com a opção ligada, toda chamada `trace_printk(...)` no código **compilado na
árvore** vira `pr_debug(...)`. O `pr_debug` é compilado fora por padrão (ou fica
sob dynamic debug), então o ring buffer deixa de ser ativado por essas chamadas.
O símbolo `__trace_printk` continua existindo para módulos que o referenciem.

A correção `trace_printk(buf)` → `trace_printk("%s", buf)` foi necessária porque,
ao expandir para `pr_debug`, o compilador passou a checar o formato e rejeitou
uma string não literal (além de ser uma falha de format string). O commit do
build fix foi dobrado neste mesmo commit.

**Importante:** o aviso de boot do `trace_printk` ainda aparece porque um módulo
vendor **pré-compilado** (`vendor_dlkm`) chama `__trace_printk`. A config só
afeta o que é recompilado aqui. O aviso é inofensivo e não indica regressão.

## Compatibilidade

- Não altera ABI, structs nem release string.
- Módulos pré-compilados que chamam `__trace_printk` continuam funcionando.
- Se algum código built-in depender de `trace_printk` para diagnóstico, ele
  deixa de imprimir; nesse caso, use dynamic debug ou desligue a opção em um
  build de debug.

## Validação

- Build `#3` sem erro de formato no `decon_core.c`.
- Boot OK; o aviso de `trace_printk` no dmesg vem do módulo vendor, não do
  código built-in.
- 271 módulos carregados.

## Como evoluir

- Não adicione `trace_printk` esperando saída em produção; use `pr_debug` com
  dynamic debug ou `dev_dbg`.
- Se for investigar um problema que exige `trace_printk` built-in, desligue
  `CONFIG_DISABLE_TRACE_PRINTK` temporariamente (build de debug), não no build
  de release.
- Ao portar código do Floppy, procure por `trace_printk` com formato não
  literal; o padrão `"%s", buf` evita o erro de build.

## Rollback

```sh
git revert 37ee11b45
```

Volta a compilar `trace_printk` de verdade e remove a opção. O `decon_core.c`
continua correto com `"%s"`.

## Créditos

**Park Ju Hyung**, **Alex Naidis** e **prabhatkrmishra** pela opção
`CONFIG_DISABLE_TRACE_PRINTK` (`ca6448e4c`); **Flopster101** pelo uso no Floppy e
pela correção do `decon_core.c` (`fa1921857`). Ver `docs/creditos.md`.
