# BakaSU no Kbuild

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `b5b15e253` |
| Arquivos | `drivers/Kconfig`, `drivers/Makefile`, `drivers/kernelsu` (symlink) |
| Origem | integração não-GKI do KernelSU/BakaSU |
| Risco | baixo |
| Validação | build `#2` e `#3` |

## Contexto

O código do BakaSU vive em `KernelSU/kernel`, fora da árvore de drivers do
kernel. Era preciso encaixá-lo no Kbuild sem duplicar arquivos e sem quebrar os
caminhos relativos que o próprio projeto usa.

## Mudança

Symlink versionado:

```
drivers/kernelsu -> ../KernelSU/kernel
```

`drivers/Kconfig`:

```kconfig
source "drivers/kernelsu/Kconfig"
```

`drivers/Makefile`:

```make
obj-$(CONFIG_KSU) += kernelsu/
```

## Mecanismo

O symlink relativo faz o Kbuild enxergar `drivers/kernelsu/` como um diretório
normal, mas os arquivos continuam vindo do submódulo. O `obj-$(CONFIG_KSU)` só
compila o diretório quando `CONFIG_KSU` está habilitado; o `source` do Kconfig
só é lido porque o symlink existe no momento da configuração.

O `Kconfig` do BakaSU usa caminhos relativos ao próprio diretório
(`KernelSU/kernel`), então o symlink também preserva essa expectativa.

## Compatibilidade

- O symlink é armazenado pelo Git com modo `120000`; não é uma cópia.
- Não altera ABI, release string nem módulos.
- `CONFIG_KSU=n` remove o diretório do build por completo.

## Validação

- `readlink drivers/kernelsu` → `../KernelSU/kernel`.
- `ls drivers/kernelsu/Kconfig` funciona após `git submodule update --init`.
- Build gera os objetos `drivers/kernelsu/*.o` e o símbolo `ksu_*`.
- `grep -c ' ksu_' out/System.map` → 355 (build validado).

## Como evoluir

- **Não** substitua o symlink por uma cópia. Uma cópia sai de sincronia com o
  submódulo e quebra a atualização.
- Em ambientes que não preservam symlinks (zip, alguns clients Windows), o
  build falha com “No such file or directory”. A solução é clonar com Git ou
  recriar o symlink.
- Se um dia o BakaSU passar a usar `drivers/kernelsu` como caminho absoluto,
  ajuste o `source`/`obj` de acordo, mas mantenha o submódulo como fonte.
- Worktrees do Git funcionam porque o symlink é relativo e o submódulo é
  inicializado por worktree.

## Rollback

```sh
git revert b5b15e253
```

Junto com o commit do submódulo, remove a integração. Sem o symlink, o
`source "drivers/kernelsu/Kconfig"` falha na configuração.

## Créditos

Integração não-GKI documentada pelo KernelSU e adaptada pelo BakaSU.
Repositório: <https://github.com/Baka-SU/BakaSU>.
