# Título curto da mudança

| Campo | Valor |
|---|---|
| Status | implementado / validado / parcial / revertido |
| Commit(s) | `hash` (branch original, se aplicável) |
| Arquivos | `caminho/do/arquivo` |
| Origem | Floppy `hash`, BakaSU, upstream ou autoral |
| Risco | baixo / médio / alto |
| Validação | build / boot / teste específico |

## Contexto

Qual problema existia, o que motivou a mudança e qual era o comportamento
anterior. Se veio de uma análise ou de um bug, cite o documento ou o sintoma.

## Mudança

O que foi alterado, de forma objetiva. Inclua o diff relevante ou o trecho de
código final quando isso ajudar a próxima pessoa a entender o ponto exato.

```c
/* trecho alterado */
```

## Mecanismo

Por que a mudança funciona. Explique o caminho quente, a estrutura de dados, a
ordem de chamadas ou a semântica do Kconfig envolvida. Registre também o custo:
tamanho de imagem, tempo de build, latência, consumo ou perda de diagnóstico.

## Compatibilidade

O que **não** pode quebrar:

- release string / vermagic dos 271 módulos da ROM;
- `CONFIG_MODVERSIONS=y`;
- CFI, PAC e shadow call stack;
- layout de structs e assinatura de símbolos exportados;
- comportamento para processos que não usam root.

Se a mudança toca algum desses pontos, explique por que é segura.

## Validação

O que já foi testado, com números e comandos quando possível:

- build: `Image` gerada, sem erro;
- boot: `sys.boot_completed=1`, release esperado;
- módulos: 271/271, sem `unknown symbol`;
- root: `su -c id` = `u:r:ksu:s0`;
- teste específico da mudança.

Liste claramente o que **ainda falta** testar.

## Como evoluir

Armadilhas, dependências e pontos de ajuste. Exemplos:

- qual valor aumentar/diminuir primeiro;
- qual config precisa estar junto;
- qual arquivo/versão pode conflitar em um rebase;
- o que medir antes de manter a mudança.

## Rollback

Como desfazer sem perder outros trabalhos:

```sh
git revert <hash>
```

Se houver impacto no aparelho, informe a imagem/hash de rollback.

## Créditos

Autor original, repositório e commit. Se o patch foi adaptado, diga o que mudou
em relação à origem. A lista completa fica em `docs/creditos.md`.
