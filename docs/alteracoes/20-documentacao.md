# Documentação do projeto

| Campo | Valor |
|---|---|
| Status | — |
| Commits | `cb7c7755d`, `62349e16d`, `4d235b83e`, `461ba783b`, `735c6995a`, `58ebe7640`, `de4e32716`, `8378fef68`, `f51681a27`, `b04510d02`, `4cb891642` |
| Arquivos | `docs/*.md`, `docs/alteracoes/*.md`, `PLAN.md` (fora do repo) |
| Origem | projeto |
| Risco | — |
| Validação | revisão manual |

## Contexto

O projeto precisa continuar em outras sessões e com outros modelos. Além do
código, é necessário registrar **como** compilar, gravar, validar e por que cada
mudança existe. Esta pasta (`docs/alteracoes/`) é a camada nova; os documentos
abaixo são a base já existente.

## Documentos

Caminhos relativos à raiz do repositório.

| Documento | Conteúdo | Commit principal |
|---|---|---|
| `docs/build-bakasu-o1s.md` | Toolchain, config, `LOCALVERSION`, build manual, verificação | `cb7c7755d` |
| `docs/boot-img-e-flash.md` | Repack do `boot.img`, AVB, gravação, rollback, userspace do BakaSU, `su` no `adb shell` | `62349e16d`, `461ba783b` |
| `docs/validacao-o1s-2026-11-08.md` | Validação do kernel com BakaSU no aparelho (build `#2`) | `4d235b83e`, `735c6995a` |
| `docs/analise-floppy.md` | Catálogo do Floppy, riscos estruturais, roadmap das fases 1–4 | `58ebe7640` |
| `docs/validacao-floppy-fase1.md` | Validação da Fase 1 (build `#3`), imagem, hashes, pendências | `de4e32716` |
| `docs/creditos.md` | Créditos completos e mapa commit local → origem Floppy | `8378fef68`, `f51681a27`, `4cb891642` |
| `docs/alteracoes/` | Um documento por mudança (esta pasta) | `—` |
| `PLAN.md` | Plano original do projeto (fora do repositório) | `—` |

O commit `b04510d02` também adicionou comentários de crédito no `Makefile`,
`lib/*/Makefile` e `net/sched/Kconfig`, para que a origem apareça no código e não
só nos documentos.

## Mudança

Criação de `docs/alteracoes/` com um documento por mudança, do índice
(`README.md`) e do template (`_template.md`). Os documentos existentes
(`docs/*.md`) foram mantidos e passaram a ser referenciados por esta pasta.

## Validação

- Revisão manual do conteúdo.
- Links relativos verificados.
- `git diff --check` sem erros.
- Não há impacto em runtime; nenhum build de kernel é necessário.

## Como evoluir

- **Toda mudança de código/config** deve vir com um documento novo em
  `docs/alteracoes/`, usando `_template.md`.
- Atualize a tabela de `docs/alteracoes/README.md` (número, link, commit, status).
- Se a mudança for de uma fase futura, marque o status como `parcial` até o teste
  no aparelho terminar.
- Se o comportamento mudar depois (por exemplo, um valor de tuning), atualize o
  documento no mesmo commit da mudança.
- Não duplique conteúdo: build/flash/validação ficam nos documentos existentes;
  `docs/alteracoes/` aponta para eles.

## Rollback

```sh
git revert <hash-do-commit-de-documentação>
```

Não há impacto em runtime. Os documentos originais continuam válidos; só a pasta
`docs/alteracoes/` e os links cruzados deixam de existir.

## O que não fazer

- Não colocar dados do aparelho de teste (serial, IMEI, MAC, SSID, número de
  telefone, hostname) em documento nenhum. Use placeholders.
- Não colocar caminhos absolutos da máquina de build (`/home/<usuário>/...`).
- Não commitar `Image`, `boot.img`, `.ko`, `out/` ou toolchains.
- Não apagar a análise do Floppy (`docs/analise-floppy.md`) depois que uma fase
  for concluída; ela é o registro do que foi avaliado e do que foi descartado.

## Créditos

Os documentos existentes já citam os autores. A lista completa e o mapeamento de
commits estão em `docs/creditos.md`.
