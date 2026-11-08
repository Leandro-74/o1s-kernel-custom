# CPU do Exynos 2100

| Campo | Valor |
|---|---|
| Status | validado |
| Commit | `690cbc40d` |
| Arquivos | `Makefile` |
| Origem | FloppyKernel `e791203f8` (Flopster101 / Floppy1280) |
| Risco | baixo |
| Validação | build `#3`, boot, 271 módulos |

## Contexto

Sem flags de CPU, o clang gera código para o baseline genérico `armv8-a`, que não
aproveita o agendamento nem as extensões disponíveis nos núcleos do Exynos 2100.
O Floppy usa um par `-mcpu`/`-mtune` para alinhar o código ao hardware.

## Mudança

No `Makefile`, depois do bloco de otimização:

```make
# CPU tuning for the Exynos 2100, from FloppyKernel
# (Floppy1280, Flopster101).
ifdef CONFIG_CC_IS_CLANG
ifeq ($(CONFIG_ARCH_EXYNOS), y)
KBUILD_CFLAGS += -mcpu=cortex-a55 -mtune=cortex-a78
endif
endif
```

## Mecanismo

- `-mcpu=cortex-a55` define o **conjunto de instruções** mínimo como o do Cortex
  A55 (ARMv8.2-A). O kernel roda nos núcleos little (A55), nos big (A78) e no
  prime (X1); usar o A55 como baseline garante que nenhuma instrução gerada seja
  inválida nos núcleos menores.
- `-mtune=cortex-a78` muda apenas o **modelo de agendamento** e as heurísticas de
  código (latências, portas, alinhamento). O A78 é próximo do X1 e bem diferente
  do A55; o resultado é um código que continua válido no A55, mas com escalonamento
  pensado para os núcleos grandes.
- A condição `CONFIG_ARCH_EXYNOS` evita aplicar a flag em builds de outras
  arquiteturas; `CONFIG_CC_IS_CLANG` evita mandar flags do clang para o GCC.

Custo: parte do aumento de ~3,3 MB da imagem do build `#3` vem daqui, junto com
unroll/LTO. Não há mudança de comportamento funcional.

## Compatibilidade

- As flags não mudam ABI, layout de structs nem o release string.
- O código continua rodando em todos os núcleos do SoC; não é `-mcpu=native`
  (que usaria o host de build, não o aparelho).
- Não habilite `-march` com extensões que o A55 não tenha (SVE, por exemplo).

## Validação

- Build `#3` bootou com `sys.boot_completed=1`.
- `uname -r` idêntico ao stock.
- 271/271 módulos carregados, sem `unknown symbol`.
- Frequências observadas: little 533 MHz em idle, prime 2,6 GHz sob carga de boot.
- Nenhum `BUG:`, panic ou falha de CFI.

## Como evoluir

- Se o alvo deixar de ser o Exynos 2100, não remova a condição
  `CONFIG_ARCH_EXYNOS` sem avaliar outros SoCs da árvore.
- Para testar `-mcpu=cortex-a78` no lugar do A55, verifique antes se todos os
  núcleos suportam as instruções geradas; o A55 é o limite seguro.
- Meça boot time, benchmarks e tamanho de imagem antes de mexer no par. Mudar
  `-mtune` não deve alterar a corretude, só desempenho.
- Não use `-mcpu=native`: o build roda no PC, não no celular.

## Rollback

```sh
git revert 690cbc40d
```

Volta ao baseline `armv8-a`. O kernel continua bootando; a diferença é só de
desempenho/tamanho.

## Créditos

Flags do **FloppyKernel**, commit `e791203f8`, de **Flopster101** e
**Floppy1280**. Ver `docs/creditos.md`.
