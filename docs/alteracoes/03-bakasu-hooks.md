# Hooks manuais do BakaSU

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `f35056027` |
| Arquivos | `fs/exec.c`, `fs/open.c`, `fs/stat.c`, `kernel/reboot.c` |
| Origem | integração não-GKI do KernelSU/BakaSU |
| Risco | médio (caminho quente de syscalls) |
| Validação | root e `su` no `adb shell` |

## Contexto

Em kernels 5.4 sem GKI 2.0 não existe o redirecionamento por tracepoint que o
KernelSU moderno usa. O caminho suportado é o **manual hook**: o kernel chama
funções do BakaSU diretamente nos pontos de entrada das syscalls relevantes.

Esses pontos já existiam na árvore como patches do KernelSU; este projeto os
mantém encapsulados em `CONFIG_KSU_MANUAL_HOOK`, para que desligar o root
restaure o comportamento stock sem reescrever os arquivos.

## Mudança

Todos os blocos são condicionados por `#ifdef CONFIG_KSU_MANUAL_HOOK`.

### `fs/exec.c`

```c
ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);
/* ... executa o binário ... */
ksu_handle_post_execveat(&fd, &filename, &argv, &envp, &flags, &retval);
```

- `execveat` pré-execução: intercepta `su`, aplica o `sucompat` e o perfil de
  root.
- `post_execveat`: trata o pós-execução (injeção de `init.rc`, ajustes de
  credenciais e retorno).

### `fs/open.c`

```c
ksu_handle_faccessat(&dfd, &filename, &mode, NULL);
```

Faz parte do `sucompat`: quando o processo atual não é privilegiado e o
`sucompat` está ativo, intercepta verificações de acesso ao caminho `su` e o
reescreve para `sh`. Assim, apps que testam a existência de `su` não encontram o
binário, mas o `su` continua funcionando para quem tem root.

### `fs/stat.c`

Hooks em `newfstatat`, `newfstat`, `fstat64` e `fstatat64`:

```c
ksu_handle_stat(&dfd, &filename, &flag);
ksu_handle_newfstat_ret(&fd, &statbuf);
ksu_handle_fstat64_ret(&fd, &statbuf);
```

- `ksu_handle_stat()` aplica a mesma troca `su` → `sh` nos caminhos de `stat`,
  antes da syscall.
- `ksu_handle_newfstat_ret()` e `ksu_handle_fstat64_ret()` rodam **depois** da
  syscall e ajustam o `st_size` relatado do `init.rc` injetado, para que o
  userspace leia o arquivo completo.

Nenhuma struct do kernel é alterada; os hooks só ajustam os argumentos/valores
de retorno da syscall.

### `kernel/reboot.c`

```c
ksu_handle_sys_reboot(magic1, magic2, cmd, &arg);
```

É o “supercall” do gerenciador. Com `KSU_INSTALL_MAGIC1`/`KSU_INSTALL_MAGIC2`,
instala o file descriptor do driver KSU no processo. Com
`CONFIG_KSU_TOOLKIT_SUPPORT` e/ou `CONFIG_KSU_SUSFS`, também trata comandos de
extensão; esses comandos exigem uid 0. A chamada acontece antes da checagem de
`CAP_SYS_BOOT` da syscall `reboot(2)`.

## Mecanismo

As funções do BakaSU recebem ponteiros para os argumentos da syscall e podem
reescrevê-los antes de a syscall continuar. Os hooks de retorno
(`newfstat`/`fstat64`) rodam depois da syscall e ajustam o valor já copiado para o
userspace. O `__attribute__((hot))` mantém essas funções na região quente do
texto, reduzindo o custo de uma chamada por syscall.

O custo é uma chamada de função condicional em caminhos muito usados (`execve`,
`stat`, `faccessat`). Como o BakaSU já era esperado no kernel, o custo é aceito;
a alternativa (tracepoints) não existe neste kernel.

## Compatibilidade

- As assinaturas precisam casar **exatamente** com as do BakaSU. O BakaSU tem um
  verificador de hooks em tempo de compilação; se ele falhar, o build para.
- Nenhuma struct do kernel é alterada. Os hooks só recebem ponteiros para os
  argumentos existentes.
- Sem `CONFIG_KSU_MANUAL_HOOK`, o código é removido pelo pré-processador e o
  kernel volta ao comportamento stock.
- `CONFIG_KSU=y` é obrigatório; `KSU_MANUAL_HOOK` depende de `KSU != m`.

## Validação

- `su -c id` → `uid=0(root)`, contexto `u:r:ksu:s0`.
- dmesg mostra o caminho:
  `do_execveat_common su found` seguido de
  `do_ksu_handle_execveat_sucompat su->sh!`.
- Root funcionou no app de teste e no `adb shell` (após autorizar
  `com.android.shell` no gerenciador).
- `pm` e comandos do gerenciador funcionaram; não foi observado erro de
  `faccessat`/`stat`.

## Como evoluir

- Ao atualizar o submódulo, compare os protótipos em
  `KernelSU/kernel/hook/` e `KernelSU/kernel/supercall/dispatch.c`. Um protótipo
  diferente gera warning ou comportamento indefinido.
- Se for adicionar um hook novo, adicione o `extern`, a chamada sob o mesmo
  `#ifdef` e documente aqui. Não deixe a chamada fora do `#ifdef`.
- Não mova a chamada de `execveat` para depois da checagem de `IS_ERR`, nem a de
  `post_execveat` para antes de `putname`; a ordem atual é a que o BakaSU espera.
- Os hooks de `stat` são sensíveis a `__ARCH_WANT_STAT64`; o bloco condicional já
  cobre as duas variantes.

## Rollback

```sh
git revert f35056027
```

O root deixa de funcionar, mas o kernel continua bootando. Reverta junto o
`CONFIG_KSU_MANUAL_HOOK` se não for usar outro modo de hook.

## Créditos

Integração não-GKI do **KernelSU** (tiann e colaboradores), mantida pelo
**BakaSU**. Documentação: <https://bakasu.org/guide/manual-integrate.html>.
