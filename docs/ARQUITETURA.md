# Caligo — Arquitetura

Versão 0.4

## 1. Princípio

A arquitetura existe para que o código continue pequeno, previsível e substituível. Três regras governam tudo:

1. Um módulo tem uma responsabilidade.
2. Dependência aponta só para baixo.
3. Nenhum arquivo cresce sem limite.

## 2. Módulos

| Módulo | Responsabilidade | Depende de |
|---|---|---|
| `core` | memória, containers, log, math, jobs, tempo, resultado | — |
| `platform` | janela, input, arquivo, threads, DPI | core |
| `render` | RHI, render graph, passes, UI renderer, pós | core, platform |
| `assets` | identidade `.cmeta`, importação, cook, cache, dependências, manifesto | core, platform |
| `build` | stage, pacote, build ID e verificação de entrega | core, platform, assets, script |
| `scene` | entidades, componentes, serialização, undo, prefabs | core, assets |
| `physics` | Jolt, queries, character, veículos | core, scene |
| `audio` | mixer, 3D, zonas, streaming | core, assets |
| `animation` | clips, blend, IK, retarget | core, scene |
| `body` | Tônus, corpo ativo, comportamento, nível de simulação | core, scene, animation, physics |
| `script` | host .NET via hostfxr, compilador Roslyn, bindings, play e recarga | core, scene, physics, audio |
| `game` | mundo, streaming, trânsito, multidão, câmera, UI | todos acima |
| `editor` | ferramentas, viewport, painéis, configurações, profiler | todos acima |

Regras de fronteira:

- Só `include/caligo/<módulo>/` é público. Nenhum módulo inclui `src/` de outro.
- Header inclui o mínimo; o resto é declaração antecipada.
- Nenhum módulo de jogo importa o editor. O editor é folha.
- O script nunca inclui header de render, física ou memória. Ele fala com uma ABI C curada.
- O host nativo carrega `hostfxr` e um bootstrap não coletável; os assemblies de gameplay vivem em um `AssemblyLoadContext` coletável com shadow copy. Detalhes em `docs/sistemas/SCRIPTING.md`.
- O compilador C# é um processo auxiliar (`Caligo.Compiler`) sobre o runtime privado; não é uma biblioteca ligada ao editor. O editor e a CLI só trocam arquivos de pedido, resposta e diagnóstico.
- O `editor` não implementa regra de negócio de asset, cook, build ou script; ele orquestra os módulos e apresenta o resultado.

## 3. Threads

- **Main:** editor, cena, input, tick de jogo.
- **Render:** submissão e sincronização com a GPU.
- **Jobs:** culling, animação, passos de física, decode de asset.
- **IO:** carregamento assíncrono de assets.

Regras:

- Nenhum lock em caminho quente.
- Dados entre threads são double-buffered ou copiados na fronteira de fase.
- Estado mutável compartilhado só existe com contrato escrito no header.
- Simulação em tick fixo de 60 Hz; render interpolado.
- Compilação e cook rodam fora da thread principal; o editor mostra progresso e pode cancelar. O resultado só é aplicado numa fronteira de quadro.
- Um cook ou build não altera a cena aberta; ele lê a fonte e grava saída.

## 4. Memória

- Arena linear por frame e por fase.
- Pool para componentes e recursos de render.
- `unique_ptr` para posse; ponteiro cru só para observação.
- `new` e `delete` proibidos no código de jogo.
- Alocador com rastreio em debug, relatório de vazamento no shutdown.
- Orçamento de memória por subsistema, verificado no CI.
- Asset cozido é carregado em bloco imutável; o runtime não mantém árvore de objetos intermediária.

## 5. Erros

- `Result<T>` para falha recuperável. Exceção não atravessa módulo.
- `CALIGO_ASSERT` para erro de programação em debug.
- Falha fatal: log, flush, dump e saída limpa.
- Nenhuma falha silenciosa. Toda falha tem log e teste.
- Importação, compilação, cook e build produzem diagnóstico com código, severidade, origem e mensagem. O console do editor e o build consomem o mesmo formato.
- Componente desconhecido falha a carga da cena com diagnóstico; o cook preserva os dados. Nunca há descarte silencioso.

## 6. Organização de arquivos

- Limite rígido por arquivo, verificado no CI: **600 linhas em `.cpp`, 400 em `.h`**.
- Código gerado e tabela de dados pura só excedem o limite por allowlist em `tools/limite_arquivos.txt`, com motivo. Entrada obsoleta quebra o CI.
- O limite é gatilho de decomposição por responsabilidade, nunca corte por número.
- Complementos: função com no máximo 80 linhas e tipo com no máximo 20 métodos públicos.
- Um tipo principal por arquivo; o nome do arquivo é o nome do tipo.
- Divisão por responsabilidade: `GBufferPass.cpp`, `ShadowPass.cpp`, `TaaPass.cpp`. Nunca um `DeferredRenderer.cpp` de sete mil linhas.
- Funções com no máximo 80 linhas e no máximo 3 níveis de aninhamento.
- Header com `#pragma once`, declaração antecipada e nenhuma implementação fora de template ou inline.
- `TODO`, `FIXME`, `stub` e `placeholder` quebram o CI.
- Sem comentários. Nome e estrutura explicam.
- Formatação por clang-format, verificado no CI.
- Nomes: PascalCase para tipos e funções, camelCase para locais, `m_` para membros, `k` para constantes. Sem prefixo `I` em interface.

## 7. Testes

- doctest, um arquivo de teste por módulo.
- Script: testes gerenciados em console, teste do host sem runtime e smoke de execução no `ctest`.
- Compilador: fonte válida, erro de sintaxe, erro de tipo, determinismo do assembly, PDB em Debug e ausência de PDB em Release.
- Assets: importação determinística, cache, dependências, manifesto e geometria inválida.
- Unitário: matemática, containers, serialização, layout.
- Integração: cena → física → snapshot de render.
- Imagem dourada: WARP, determinística, 300 frames para sistemas temporais.
- Determinismo: tick fixo, RNG com semente, relógio de parede proibido na simulação.
- Desempenho: contadores por sistema; regressão acima de 10% quebra o CI.
- CI: Windows, Debug e Release, todos os testes, artefatos publicados.

## 8. Documentação

- `docs/ESCOPO.md` — produto, escopo, releases.
- `docs/ARQUITETURA.md` — contratos entre módulos e regras transversais.
- `docs/FORMATOS.md` — formatos de origem e cozidos, schemas e versionamento.
- `docs/DECISOES.md` — registro de decisões.
- `docs/sistemas/EDITOR.md` — ferramentas, viewport, configurações e identidade visual.
- `docs/sistemas/ASSETS.md` — identidade, importação, validação, cook e navegador.
- `docs/sistemas/BUILD.md` — projeto, build, stage, modo jogo e distribuição.
- `docs/sistemas/RENDER.md` — pipeline, passes, iluminação, sombras e pós.
- `docs/sistemas/SCRIPTING.md` — modelo de script, compilação, play e hospedagem.
- `docs/sistemas/UI.md` — UI de jogo, markup, layout e binding.
- `docs/manual/` — o que a release atual entrega ao usuário.
- `docs/rfcs/` — mudanças em decisão fixa.

A documentação segue a mesma regra do código: um arquivo, uma responsabilidade. Um escopo de cinco mil linhas é o mesmo defeito que um renderer de cinco mil linhas.

## 9. Portões de CI

- Build `/W4 /WX` em Debug e Release.
- Testes unitários e de integração.
- Imagens douradas.
- Compilação do jogo de exemplo pelo compilador embutido, sem MSBuild.
- Cozimento determinístico do jogo de exemplo: dois cooks, o mesmo manifesto.
- Verificação de que o stage não contém fonte.
- Limite de linhas por arquivo.
- Proibição de TODO, FIXME, stub e placeholder.
- clang-format.
- Checagem de fronteira de módulo.

O CI valida a engine e o jogo de exemplo. Ele não é o build de um jogo e não é requisito para produzir um.

## 10. Histórico

- **0.4** — adiciona `build`, detalha assets e compilador, separa o processo `Caligo.Compiler`, atualiza testes, documentação e portões de CI.
- **0.3** — módulos, threads, memória, erros, arquivos, testes e documentação da R2.
- **0.2** — fronteira de script, hospedagem privada e organizações de arquivo.
- **0.1** — fundação da arquitetura.
