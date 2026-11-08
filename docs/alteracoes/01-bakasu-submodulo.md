# BakaSU como submódulo

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `ab62a7d55` |
| Arquivos | `.gitmodules`, `KernelSU` (gitlink) |
| Origem | BakaSU / KernelSU |
| Risco | baixo |
| Validação | build `#2` e `#3`, root no aparelho |

## Contexto

O kernel precisa de root em nível de kernel. O BakaSU é o fork do KernelSU usado
neste projeto: ele mantém suporte a kernels antigos (3.4+) via hooks manuais, que
é o caminho necessário no Exynos 2100 com Linux 5.4 e sem GKI 2.0.

O código não foi copiado para dentro da árvore. Ele entra como submódulo para
manter o histórico e a licença do projeto original, e para deixar explícito qual
commit exato está sendo compilado.

## Mudança

`.gitmodules`:

```ini
[submodule "KernelSU"]
	path = KernelSU
	url = https://github.com/Baka-SU/BakaSU
```

O gitlink aponta para:

```
516d3e87fbc986ccefdde2c2765f1700612441a9
```

Esse commit corresponde ao BakaSU `v4.2.0-rc3-86`, driver `35257`, uapi 6.

## Mecanismo

O submódulo é baixado em `KernelSU/`. O Kbuild do kernel o consome pelo symlink
`drivers/kernelsu` (documento 02). A versão do driver fica gravada no gitlink,
então qualquer clone com `--recursive` compila exatamente o mesmo código.

O gerenciador instalado no aparelho precisa ter a mesma versão de driver
(`35257`). Versões diferentes mostram “Manager update required” e não criam o
userspace em `/data/adb/ksud`.

## Compatibilidade

O submódulo não altera ABI, structs nem o release string. Ele só adiciona código
condicionado a `CONFIG_KSU`.

## Validação

- `git submodule status` mostra `516d3e87... KernelSU (v4.2.0-rc3-86-g516d3e87)`.
- Driver reportado no aparelho: `35257`, `v4.2.0-rc3-516d3e87`, uapi 6.
- `su -c id` retorna `uid=0(root)` com contexto `u:r:ksu:s0`.

## Como evoluir

- Para atualizar o BakaSU: troque o commit do submódulo, rode
  `git submodule update --init`, confira se os protótipos dos hooks manuais
  continuam iguais (documento 03) e recompile.
- O BakaSU tem um verificador de hooks em tempo de compilação. Se ele reclamar,
  leia a mensagem antes de mascarar o erro.
- Não commite o conteúdo de `KernelSU/` como arquivos normais; ele deve
  continuar como gitlink.
- Clones sem `--recursive` deixam `KernelSU/` vazio e o build falha. Rode
  `git submodule update --init` antes de compilar.

## Rollback

```sh
git revert ab62a7d55
```

Isso remove o submódulo e o gitlink. Os hooks e o Kconfig (documentos 02 a 04)
precisam ser revertidos junto, senão o build quebra.

## Créditos

- **KernelSU**: tiann e colaboradores.
- **SukiSU Ultra**, **ReSukiSU** e **BakaSU**: mantenedores dos forks.
- Repositório usado: <https://github.com/Baka-SU/BakaSU>.
