# AGENTS.md — kernel o1s (LineageOS 23.2 + BakaSU)

Guia para qualquer agente ou sessão que for mexer neste repositório. Leia este
arquivo **antes** de alterar código, config, build ou documentação.

## 1. Leitura obrigatória

1. Este `AGENTS.md`.
2. `docs/alteracoes/README.md` — índice de todas as mudanças já feitas.
3. O documento específico da área que você vai mexer em `docs/alteracoes/`.
4. `docs/analise-floppy.md` — o que foi avaliado, o que foi descartado e o
   roadmap das fases.
5. `docs/build-bakasu-o1s.md` — toolchain, config e build.
6. `docs/boot-img-e-flash.md` — empacotamento, gravação e rollback.
7. `docs/creditos.md` — autores e mapa commit local → origem.

## 2. Mapa do repositório

| Caminho | O que é |
|---|---|
| `arch/arm64/configs/samsung/o1s.config` | Fragmento de config do dispositivo (registro versionado) |
| `out/.config` | Config efetiva do build manual (não versionada) |
| `Makefile` | Flags de CPU, LTO e codegen |
| `KernelSU/` | Submódulo BakaSU (gitlink `516d3e87`) |
| `drivers/kernelsu` | Symlink para `../KernelSU/kernel` |
| `docs/` | Build, flash, análises, validação e créditos |
| `docs/alteracoes/` | Um documento por mudança |
| `ksu-modules/` | Fontes dos módulos do KernelSU (scripts e painel WebUI) |
| `out/` | Saída de build (ignorada pelo Git) |

Fora deste repositório, no diretório pai:

- `../toolchains/` — AOSP Clang `r563880c` e binutils.
- `../baseline-build/` — worktree do baseline upstream.
- `../PLAN.md` — plano original do projeto.
- `../boot-work/` — imagens de boot e backups.

## 3. Estado atual (não assuma outro)

| Item | Valor |
|---|---|
| Base upstream | `a273fe61262c` — tag `lineage-23.2-20261031`, Linux 5.4.302 |
| Branch padrão | `resukisu` |
| HEAD da Fase 1 | `4cb891642` (antes dos commits de documentação) |
| Release esperado | `5.4.302-android12-5.4-exynos-ga273fe61262c` |
| Root | BakaSU `516d3e87`, driver `35257`, uapi 6 |
| Build validado | `#3` (`boot-floppy-fase1.img`) |
| Módulos da ROM | 271/271 |
| Fase 1 | mesclada em `resukisu`; branch `floppy-tweaks` removida (local e remota). Testes longos seguem na `resukisu` |
| Undervolt | módulo `undervolt_o1s.ko` + painel KernelSU; ver `docs/alteracoes/22-undervolt-modulo-kernel.md` |

Pendências conhecidas: drain em idle de 8 h, suspend/resume longo, teste real de
tethering/TTL, pressão de memória para o kswapd e térmico sob carga para o
`energy_step`.

## 4. Regras obrigatórias do usuário

1. **Branch de teste separada.** Toda implementação que precisa ser validada
   deve ser feita em uma branch própria, criada a partir de `resukisu` e nomeada
   conforme a implementação (ex.: `undervolt-runtime`, `thermal-offsets`,
   `mglru`). Não implemente direto na `resukisu`.
2. **Merge e remoção após validação total.** Quando a implementação for
   totalmente validada no aparelho, faça o merge na `resukisu` e apague a branch
   de teste (local e, após confirmação, remota). Se a validação ainda tiver
   pendências, a branch continua existindo e o status fica `parcial` na
   documentação. Testes de longa duração (drain, suspend, uso normal) podem
   continuar na `resukisu` depois do merge; se surgir inconsistência, crie uma
   branch de correção (ex.: `fix-<assunto>`) em vez de reabrir a branch antiga.
3. **Commits por alteração.** Divida o trabalho em commits atômicos, um por
   mudança lógica. Use `git add -p` quando um arquivo tiver mais de uma mudança
   independente. Não junte mudanças não relacionadas.
4. **Nada sensível ou exclusivo do aparelho de teste no código.** Não coloque
   serial, IMEI, MAC, SSID, número de telefone, hostname, caminho absoluto da
   máquina, token ou qualquer dado pessoal em código, config, documento, commit
   ou mensagem. Use placeholders. Segredos, se existirem, ficam só em `.env`
   (que deve estar no `.gitignore`), com `.env.example` de valores fictícios.
5. **Push só com confirmação.** Ao terminar a tarefa e criar os commits, pare e
   aguarde a confirmação explícita do usuário para dar push. Nunca faça push
   sozinho e nunca use `--force` em branch publicada.

## 5. Regras técnicas do projeto

### Compatibilidade com a ROM

- **Não altere o release string.** Ele precisa continuar
  `5.4.302-android12-5.4-exynos-ga273fe61262c`. É o vermagic dos 271 módulos
  pré-compilados de `vendor_dlkm`.
- Mantenha `CONFIG_MODVERSIONS=y`, `CONFIG_LTO_CLANG=y`, `CONFIG_THINLTO=y`,
  `CONFIG_CFI_CLANG=y`, `CONFIG_SHADOW_CALL_STACK=y` e
  `CONFIG_ARM64_PTR_AUTH=y`.
- **Não desligue CFI, PAC, shadow call stack, KFENCE ou SLUB_DEBUG** para ganhar
  desempenho. O ganho não compensa a perda de segurança e de diagnóstico.
- **Não mude layout de structs** nem assinaturas de símbolos exportados. Os
  módulos da ROM não são recompilados.
- Não habilite opções que mudem `struct task_struct`, `struct module` ou o ABI
  GKI “por convenience”. Se um patch exigir isso, descarte ou adapte.

### Config

- Toda opção de config deve ser registrada em
  `arch/arm64/configs/samsung/o1s.config` (versionado) **e** aplicada em
  `out/.config` para o build manual. Os dois precisam ficar em sincronia.
- Depois de mexer na config, rode `olddefconfig` e confira os símbolos com
  `grep` antes de compilar.
- `CONFIG_KPROBES=y` é o estado validado. Se o aparelho entrar em safe mode ou o
  root ficar instável, a primeira tentativa é desligar kprobes e documentar o
  resultado; não mexa em outra coisa antes disso.
- `CONFIG_KSU=y` e `CONFIG_KSU_MANUAL_HOOK=y` são obrigatórios para o root
  atual. `KSU_MANUAL_HOOK` depende de `KSU != m`.

### BakaSU

- O submódulo fica em `KernelSU/`, commit `516d3e87`. Atualizações são
  intencionais: troque o commit, rode `git submodule update --init`, confira os
  protótipos dos hooks e recompile.
- Não substitua o symlink `drivers/kernelsu` por uma cópia.
- Ao atualizar o gerenciador, mantenha a mesma versão de driver (`35257`).
- Não ligue `CONFIG_KSU_DEBUG` só para liberar `adb shell`; autorize
  `com.android.shell` no gerenciador.

### Build

- Use o toolchain AOSP Clang `r563880c` (21.0.0) e LLD 21.
- Sempre exporte `LOCALVERSION=` para não ganhar sufixo `+` no release.
- Antes de recompilar, apague `out/include/config/kernel.release` e
  `out/include/generated/utsrelease.h` se o release tiver mudado.
- Não use `make gki_defconfig` como ponto de partida: sem detecção de compilador
  ele desliga LTO/CFI e quebra a compatibilidade com os módulos.
- Não commite `Image`, `boot.img`, `.ko`, `out/` nem toolchains.

### Código e patches externos

- Ao portar algo do Floppy, use `git cherry-pick -x` quando o patch aplicar
  limpo. Quando precisar adaptar, cite o hash de origem e o autor no documento
  da mudança e, se possível, em um comentário no código.
- Mantenha os créditos e a licença GPL-2.0. Código derivado não é relicenciado.
- Não porte LZ4K/LZ4KD (licença Huawei “All rights reserved”) nem o padding de
  ABI GKI do Floppy.

## 6. Fluxo de trabalho

1. **Entenda**: leia o documento da área e o estado atual.
2. **Crie a branch** a partir de `resukisu`:
   ```sh
   git switch resukisu
   git switch -c <nome-da-implementacao>
   ```
3. **Implemente em commits pequenos**. Se um arquivo tiver mudanças
   independentes, use `git add -p`.
4. **Compile** (teste mínimo) e, se a mudança for de runtime, valide no aparelho.
5. **Documente** em `docs/alteracoes/<numero>-<nome>.md`, usando
   `_template.md`, e atualize a tabela do `README.md` da pasta.
6. **Commite** com o formato abaixo. O commit de código e o de documentação
   podem ser separados, mas a documentação deve entrar antes do merge.
7. **Peça validação** ao usuário quando precisar do aparelho. Avise que o Galaxy
   S21 precisa estar conectado antes de qualquer comando `adb`/flash.
8. **Após validação total**: merge na `resukisu`, apague a branch de teste,
   atualize o status na documentação e aguarde a confirmação para o push. Os
   testes de longa duração seguem na `resukisu`; correções vão em branch própria.

## 7. Build (comandos de referência)

```sh
export PATH="$PWD/../toolchains/bin:$PWD/../toolchains/clang-r563880c/bin:$PATH"

LOCALVERSION= make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
LOCALVERSION= make ARCH=arm64 O=out LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)" Image
```

Verificação rápida:

```sh
cat out/include/config/kernel.release
# 5.4.302-android12-5.4-exynos-ga273fe61262c

grep -E '^CONFIG_(KSU|KSU_MANUAL_HOOK|LTO_CLANG|THINLTO|CFI_CLANG|SHADOW_CALL_STACK|MODVERSIONS|KPROBES)=' out/.config
```

Empacotamento e gravação: `docs/boot-img-e-flash.md`. Não grave partição nem
reinicie o aparelho sem confirmação explícita do usuário.

## 8. Validação no aparelho

Antes de qualquer passo com o aparelho, **avise o usuário que o Galaxy S21
precisa estar conectado** e espere a confirmação.

Checklist mínimo por build:

- [ ] `sys.boot_completed=1`.
- [ ] `uname -r` = release esperado.
- [ ] 271/271 módulos, sem `unknown symbol`/`disagrees about version`.
- [ ] Root: `su -c id` = `uid=0(root)`, contexto `u:r:ksu:s0`.
- [ ] `dmesg` sem panic, `BUG:`, falha de CFI ou WARNING novo em relação ao
      baseline (`sched.h:1122`, `mali_kbase_mem_linux.c:1238` e
      `memcontrol.c:1317` já existem no stock).
- [ ] Wi-Fi, Bluetooth, câmera, áudio, chamadas, sensores e ZRAM.
- [ ] Para mudanças de energia: drain em idle e suspend/resume.
- [ ] Para mudanças de rede: `sysctl net.ipv4.tcp_congestion_control` e
      `tc qdisc show`.
- [ ] Para mudanças de memória: pressão controlada e `vmstat`.

Registre o resultado em `docs/validacao-*.md` ou no documento da mudança. Se
algo falhar, não faça merge; documente o sintoma e o rollback.

## 9. Documentação

- Use `docs/alteracoes/_template.md`. O documento precisa responder: o que
  mudou, por que, como funciona, o que pode quebrar, o que foi validado, o que
  falta, como evoluir e como reverter.
- Um documento por mudança, não um por commit. Se dois commits implementam a
  mesma mudança (ex.: implementação + correção de build), agrupe no mesmo
  documento e cite os dois hashes.
- Atualize `docs/alteracoes/README.md` (índice) sempre que criar/renomear um
  documento.
- Se a mudança for revertida, não apague o documento: marque `revertido` e
  explique o motivo.
- Não duplique build/flash/validação; link para os documentos existentes.

## 10. Git

Formato de commit (do AGENTS global):

```
<type>[scope]: <descrição em português>
```

- `type`: `feat`, `fix`, `refactor`, `perf`, `build`, `config`, `docs`, `test`,
  `chore`.
- `scope`: área curta (`kernel`, `sched`, `mm`, `net`, `libs`, `boot`, `trace`,
  `debug`, `bakasu`, ...).
- Sem corpo. Créditos e detalhes vão nos documentos e nos comentários de código.
- Exemplos: `perf(mm): acorda apenas um kswapd por pedido`,
  `config(net): usa CAKE como qdisc padrão`.

Antes de commitar:

- build mínimo passou (ou é commit só de documentação);
- `git diff --check` sem erros;
- documentação atualizada;
- nenhum dado sensível ou binário no staging.

Branches:

- `resukisu` é a branch padrão e o destino dos merges.
- `floppy-tweaks` foi mesclada em `resukisu` e removida (local e remota). Os
  testes de longa duração seguem na `resukisu`; se aparecer inconsistência, crie
  uma branch de correção com nome descritivo.
- Branches de teste novas seguem o nome da implementação. Após o merge
  validado, apague a branch local e, com confirmação, a remota.

## 11. Dados sensíveis

- Nada de dados pessoais ou do aparelho em código, config, docs, commits ou
  mensagens.
- Use placeholders (`<serial>`, `<ssid>`, `<ip>`) em exemplos.
- Segredos só em `.env`, com `.env` no `.gitignore` e `.env.example` contendo
  apenas nomes e valores fictícios.
- Não cole saída de `adb` que contenha serial, IMEI, MAC ou número de telefone.

## 12. Créditos e licença

- Todo patch portado mantém o crédito do autor original e a licença GPL-2.0.
- Atualize `docs/creditos.md` ao portar algo novo.
- A lista de origem por commit está em `docs/creditos.md`; use-a para preencher
  o campo “Origem” do documento da mudança.
- Não remova comentários de crédito do `Makefile`, `lib/*/Makefile` ou
  `net/sched/Kconfig`.

## 13. O que não fazer

- Não desligar CFI/PAC/shadow call stack/KFENCE/SLUB_DEBUG.
- Não mudar layout de structs, ABI ou release string.
- Não commitar binários ou `out/`.
- Não fazer push sem confirmação; não fazer force push.
- Não apagar branch publicada, reescrever histórico ou fazer `git reset --hard`
  em trabalho não commitado sem confirmação.
- Não usar `-mcpu=native` nem flags que o A55 não suporte.
- Não ligar `CONFIG_KSU_DEBUG` para obter root no `adb shell`.
- Não gravar partição, reiniciar ou apagar dados do aparelho sem confirmação.
- Não colocar dados do aparelho de teste em nenhum lugar público.
