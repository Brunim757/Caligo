# Caligo — Escopo e Fundação

> Engine de mundo aberto estilizado. Windows. Cidade densa e natureza estilizada. Nada além.

Versão 1.4

## 1. Identidade

Caligo existe para produzir dois tipos de mundo:

- **Cidade densa** — quarteirões, ruas, veículos, trânsito, multidão, interiores.
- **Natureza estilizada** — terreno aberto, vegetação, escalada, planeio, luz de céu aberto.

A referência de qualidade é a RAGE para cidade densa. A ambição de identidade é ser a melhor engine de mundo estilizado — o campo onde uma equipe pequena pode vencer.

"Comparável à RAGE" não é adjetivo. É medido por:

- 1 km de visão sem hitch.
- 200 NPCs e 50 veículos num quarteirão a 60 fps.
- Streaming contínuo, sem tela de carregamento.
- Iteração criar → jogar em menos de um minuto.
- Conteúdo importado, cozido e empacotado sem sair da máquina do desenvolvedor.

## 2. Decisões fixas

Estas decisões não mudam sem RFC. A regra está na seção 11.

| ID | Decisão | Status |
|---|---|---|
| D-001 | Produto primeiro: nenhum sistema entra se não aparecer numa cena jogável | FIXO |
| D-002 | Duas colunas: cidade densa e natureza estilizada | FIXO |
| D-003 | Núcleo em C++20, gameplay em C# (.NET 10 LTS) | FIXO |
| D-004 | Windows 10/11 x64 e Direct3D 11 na v1 | FIXO |
| D-005 | Um executável com editor e runtime, cena como fonte única de verdade | FIXO |
| D-006 | Sem placeholder, stub ou código morto | FIXO |
| D-007 | TDD; compilação e teste da engine apenas no CI | FIXO |
| D-008 | Todo sistema visual tem teste de imagem determinístico | FIXO |
| D-009 | Sem comentários no código | FIXO |
| D-010 | Decisão fixa só muda por RFC escrito | FIXO |
| D-011 | Nome: Caligo, sem sufixo | FIXO |
| D-012 | Sem rede, VR, mobile, console ou macOS na v1 | FIXO |
| D-013 | Sem visual scripting | FIXO |
| D-014 | Sem engine genérica, marketplace ou plugins de terceiros | FIXO |
| D-015 | Sem ray tracing obrigatório e sem fotorrealismo | FIXO |
| D-016 | Stack de bibliotecas da seção 5, sem vcpkg | FIXO |
| D-017 | Asset fonte em formato aberto, cozido em binário próprio | FIXO |
| D-018 | Meta de 1080p60 numa GTX 1060 6 GB | FIXO |
| D-019 | Piso de hardware: FL11_0, 2 GB de VRAM, 8 GB de RAM | PROVISÓRIO |
| D-020 | UI de jogo separada do editor: markup declarativo, estilo em folha e binding por view-model | FIXO |
| D-021 | Limite rígido de linhas por arquivo, verificado no CI | FIXO |
| D-022 | Valor do limite: 600 linhas em `.cpp`, 400 em `.h` | FIXO |
| D-023 | Código proprietário; repositório público apenas para o CI gratuito | FIXO |
| D-024 | JSON de cena e ferramentas: nlohmann/json 3.12.0 | FIXO |
| D-025 | Coordenadas canhotas, com +X à direita, +Y para cima e +Z para frente | FIXO |
| D-026 | Play-in-editor usa snapshot, tick fixo de 60 Hz, pausa e avanço de quadro | FIXO |
| D-027 | Hospedagem de script framework-dependent com runtime .NET privado, contexto coletável e shadow copy | FIXO |
| D-028 | Projeto como descritor de conteúdo, com raiz, cena inicial e assembly de gameplay próprios | FIXO |
| D-029 | Corpo ativo como tecnologia própria, com nome Tônus, em módulo próprio e afinado por dados | FIXO |
| D-030 | Câmera como componente de cena, com lente e seleção da câmera ativa; comportamento em script C# | FIXO |
| D-031 | Editor por ferramentas: escolher a ferramenta reorganiza o editor; o viewport só faz interação 3D; play é global | FIXO |
| D-032 | Identidade visual de 13 px, espaçamento compacto e ícones em três camadas — Lucide na UI, Fluent UI System Icons nos assets e conjunto Caligo próprio para os tipos da engine — sobre ImGui e ImGuizmo | FIXO |
| D-033 | Céu, hora, clima, exposição e sombras pertencem à ferramenta Ambiente e ao ambiente da cena | FIXO |
| D-034 | Configurações em quatro seções — projeto, cena, editor/usuário e render — com dados em `.cproject`, `.cscene` e `%APPDATA%/Caligo/editor.json` e precedência engine → projeto → cena → pré-visualização | FIXO |
| D-035 | Registro de componentes com tipos estáveis `caligo.*`; transform obrigatório e único; botão Adicionar componente | FIXO |
| D-036 | Identidade de asset importado por sidecar `.cmeta` com GUID; a cena guarda o GUID no próprio arquivo; mover e renomear não perdem referência; GUID nunca é reutilizado | FIXO |
| D-037 | Importação é etapa determinística do cook; fonte, configurações e versão do importador produzem os mesmos bytes | FIXO |
| D-038 | glTF 2.0 é o único formato de modelo da v1; FBX, Draco e BasisU ficam fora | FIXO |
| D-039 | LOD, tangentes, UV de lightmap, colisão e compressão são automáticos, com override por asset | FIXO |
| D-040 | O contêiner cozido cobre modelo, esqueleto, animação, colisão, navmesh, HLOD e terreno; malha v2 com UV1, skinning, submalhas e morph targets | FIXO |
| D-041 | Scripting por projeto com Roslyn embutido na engine; compilar ao salvar, diagnóstico, último assembly válido, PDB e F5/F8; sem SDK, MSBuild, NuGet ou CI no jogo | FIXO |
| D-042 | Produzir um jogo é local e autossuficiente; CI existe só para desenvolver e validar a engine | FIXO |
| D-043 | Build local por CLI: cook, compilar scripts, montar stage e zip; um executável com dois modos; só cozido; manifesto; build ID por conteúdo | FIXO |
| D-044 | Terreno por tiles, heightfield com escultura e camadas de material, importação/exportação, LOD, colisão e vegetação | FIXO |
| D-045 | Renderer evolui em estágios, sem reescrita monolítica: PBR, CSM, velocity TAA, cel, clusters, SSR, névoa e pós | FIXO |
| D-046 | v1 só Windows x64; port é projeto pós-v1 com backend, plataforma, pacote e testes próprios | FIXO |

## 3. Não-objetivos

Caligo não é e não tenta ser:

- Engine genérica ou plataforma para terceiros.
- Marketplace, loja de assets ou ecossistema de plugins.
- 2D, mobile, VR, console ou macOS.
- Rede, MMO ou co-op na v1.
- Visual scripting.
- Fotorrealismo ou ray tracing obrigatório.
- Compatível com o protótipo Kizuri. Nenhum código dele é copiado.
- Instalador, auto-atualização, loja ou assinatura de código na v1.
- CI por jogo. O CI pertence à engine.

## 4. Plataforma e hardware

**Sistema:** Windows 10 21H2+ e Windows 11, x64.

**API gráfica:** Direct3D 11, feature level 11_0. A RHI é desenhada para receber um backend DX12 depois, mas a v1 não tem DX12.

**Compilador nativo:** MSVC 2022 (v143), C++20, `/W4 /WX`.

**Build da engine:** CMake + Ninja. Dependências por FetchContent com tag fixa. Sem vcpkg.

**Script:** .NET 10 LTS. Assembly por projeto. Runtime privado no artefato; hospedagem framework-dependent com contexto coletável.

**Compilador C#:** Roslyn embutido no artefato da engine, executado em processo auxiliar. O projeto do jogo não exige SDK, MSBuild, NuGet ou restore.

**Build do jogo:** local, pelo editor ou pela CLI, sem CI. Gera pasta e zip com runtime privado e CRT estática.

**Hardware:**

- Piso: FL11_0, 2 GB de VRAM, 8 GB de RAM.
- Alvo: GTX 1060 6 GB, 1080p60.
- Recomendado: RTX 3060 ou RX 6600.

## 5. Stack

| Área | Escolha |
|---|---|
| Janela e input | Win32 + Raw Input |
| Matemática | DirectXMath |
| Render | RHI própria sobre D3D11, HLSL |
| Física | Jolt |
| Jobs | enkiTS |
| Áudio | miniaudio + mixer 3D próprio |
| Editor | Dear ImGui docking + ImGuizmo |
| Ícones | Lucide 1.68.0 (UI, ISC) e Fluent UI System Icons 1.1.345 (assets, MIT), mais conjunto Caligo próprio, em atlas embutido |
| JSON | nlohmann/json 3.12.0 |
| glTF | cgltf |
| Imagens | stb + DirectXTex (BCn) |
| Compressão | zstd |
| Assets cozidos | FlatBuffers |
| Malha | meshopt, mikktspace, xatlas |
| Script | .NET 10 LTS; Roslyn embutido, sem SDK no jogo |
| Testes | doctest |

Cortados do protótipo: libwebp, Draco, BasisU. Não servem ao escopo e só aumentam o build.

## 6. Arquitetura

Módulos, com dependência apenas para baixo. O grafo completo, os contratos e as regras de fronteira estão em `docs/ARQUITETURA.md`.

- **core:** memória, containers, log, math, jobs, tempo.
- **platform:** janela, input, arquivo, threads.
- **render:** RHI, render graph, deferred clustered, sombras, TAA, pós.
- **assets:** importação, cook, GUID, manifesto, cache, dependências.
- **build:** stage, pacote, build ID e verificação de entrega.
- **scene:** entidades, componentes, serialização, undo, prefabs.
- **physics:** Jolt, queries, character, veículos.
- **audio:** mixer, 3D, zonas.
- **animation:** clips, blend, IK, retarget.
- **body:** Tônus, corpo ativo, comportamento e nível de simulação por distância.
- **script:** host .NET, compilador Roslyn, bindings e recarga.
- **game:** mundo, streaming, trânsito, multidão, câmera.
- **editor:** ferramentas, viewport, painéis, configurações, profiler.

**Fronteira do editor:** o editor é folha e organiza o trabalho por ferramenta (D-031). Nenhum sistema de jogo importa o editor, e nenhuma UI de jogo é desenhada com ImGui.

**Fluxo de cena:** o editor edita a cena. Play congela um snapshot, roda o mundo e descarta ao parar. Nada do runtime contamina a autoria.

**Assets:** fonte em `content/`, cozido em `cooked/`. GUID por asset importado no sidecar `.cmeta`; a cena guarda o GUID no próprio arquivo. Hash da fonte, importador versionado e cook incremental. O runtime só lê cozido.

**Script:** assembly por projeto (`MeuJogo.Gameplay.dll`). ABI C curada, wrappers C# escritos à mão no início, geração automática quando a superfície crescer. A engine compila com Roslyn embutido em processo auxiliar, pelo editor ou pela CLI; o host usa runtime .NET privado, contexto coletável e shadow copy. Sem scripts, a engine roda igual.

**Build:** o mesmo executável em modo editor e modo jogo (D-005). O build local cozinha, compila scripts, monta o stage e compacta; o CI da engine valida a engine e o jogo de exemplo, nunca o jogo do usuário.

**Determinismo:** simulação em tick fixo de 60 Hz, render interpolado; cook e build determinísticos por conteúdo.

## 7. Capacidades

### Mundo

- Streaming por células, sem hitch acima de 16 ms.
- Visão de 1 km com HLOD.
- LOD automático por distância e por cobertura em tela.
- Terreno com clipmap, 4 km² na fatia.
- Vegetação instanciada, 100 mil instâncias.
- Céu procedural, ciclo dia/noite, clima.

### Cidade

- Quarteirões modulares de 100×100 m.
- Prédios com interior e streaming de interior.
- 200 NPCs com LOD de comportamento.
- 50 veículos simultâneos.
- Trânsito em grafo de ruas, faixas, semáforos e ultrapassagem.

### Personagem e câmera

- Cápsula com step, rampa, agachar, correr, pular, escalar e planar.
- Câmera em terceira pessoa, ombro, veículo e cinematográfica.
- Animação com blend 1D/2D, layers, IK de membros, retargeting e root motion.
- Corpo ativo (Tônus): equilíbrio, reflexo, cambaleio, agarrar e levantar, compostos por comportamento, com nível de simulação por distância.

### Render

- Deferred clustered, PBR metallic-roughness.
- Cel shading por material, com contorno.
- Sombras cascateadas com PCSS.
- TAA com velocity buffer e teste de imagem.
- Fog volumétrico, bloom, DOF, motion blur, exposição e color grading.
- SSR em superfícies molhadas.
- 1080p60 no hardware alvo.

### Áudio

- Mixer 3D, 64 vozes, oclusão, reverb por zona, ducking e streaming de música.

### Editor e ferramentas

- Hub de projetos para criar, abrir, alternar e listar recentes (D-028).
- Editor por ferramentas: Objetos, Mundo, Ambiente, Animação, Veículo, UI e Configurações (D-031).
- Viewport só de interação 3D: seleção, gizmo, escultura, pintura e órbita. Parâmetros moram no painel da ferramenta.
- Identidade visual de 13 px, espaçamento compacto e ícones em três camadas: Lucide na UI, Fluent UI System Icons nos assets e conjunto Caligo próprio para os tipos da engine (D-032).
- Inspetor com Adicionar componente e registro de tipos `caligo.*` (D-035).
- Configurações de projeto, cena, editor e render, com precedência definida (D-034).
- Terreno profissional: tiles, escultura, camadas de material, heightmap de 16 bits, clipmap, colisão e vegetação (D-044).
- Ambiente com hora, céu, clima, exposição, sombras e névoa (D-033).
- Asset browser com `.cmeta`, importação, reimportação e configurações por asset (D-036).
- Projetos com raiz de conteúdo, cena inicial e assembly próprios, criados e alternados pelo editor.
- Prefabs, undo/redo e multi-seleção.
- Play-in-editor com recarga de C# e snapshot.
- Compilação automática de C# pelo editor, com diagnóstico, PDB e IDE externo (D-041).
- Cook e build locais por linha de comando, com stage e zip (D-042, D-043).
- Pacote autossuficiente para Windows x64, com runtime .NET privado e CRT estática.
- Profiler e ferramentas de produção.

### UI de jogo

- Markup declarativo, estilo em folha e binding para view-models em C#.
- Layout por âncora, pilha, grade e scroll, em unidade independente de densidade.
- HUD, menus, diálogos, notificações, marcadores de mundo e minimapa.
- Localização por chave, acessibilidade e escala de UI.
- Editor visual com pré-visualização ao vivo.
- Orçamento: menos de 1 ms de CPU e de GPU por frame.

Detalhes em `docs/sistemas/UI.md`.

### Conteúdo

- glTF entra, o motor cozinha.
- LOD, colisão, UV de lightmap e navmesh automáticos, com override por asset.
- Importação determinística e diagnóstico no cook, nunca no runtime.
- Um quarteirão autoral entra em horas, não em semanas.

## 8. Qualidade

- Todo módulo tem teste unitário.
- Todo sistema visual tem teste de imagem em WARP.
- Física e simulação determinísticas em tick fixo.
- Cook e build determinísticos por conteúdo.
- Orçamento de frame: 16,6 ms de CPU e 16,6 ms de GPU.
- Build limpo com `/W4 /WX`.
- Nenhum sistema é considerado pronto sem estar exposto no editor e dentro do orçamento.
- Nenhum jogo depende de CI, rede ou SDK instalado para ser produzido.

## 9. Riscos e provas

| Risco | Prova | Release |
|---|---|---|
| Hospedagem .NET e hot reload | Script roda, recarrega e não vaza | R1 |
| Estabilidade temporal | Imagem sem tremor em 300 frames | R2 |
| Compilação embutida | Projeto de exemplo compila sem SDK, com diagnóstico e determinismo | R3 |
| Importação e cook | Dois cooks da mesma fonte produzem o mesmo manifesto | R3 |
| Terreno profissional | Escultura, camadas, heightmap e colisão num tile de 512 m | R3 |
| Streaming sem hitch | Travessia de 1 km com frame p99 abaixo de 16 ms | R5 |
| Multidão e trânsito | 200 NPCs e 50 carros a 60 fps | R5 |
| UI de produção | HUD e menus abaixo de 1 ms por frame | R6 |
| Pipeline de conteúdo | Quarteirão autoral em menos de 8 horas | R7 |
| Entrega local | Pacote abre numa máquina sem SDK nem runtime .NET instalado | R7 |

Se uma prova falhar, o sistema muda de abordagem. A engine não recomeça.

## 10. Releases

**R0 — Fundação.** Repositório, build, CI, core, plataforma, janela, input, log, math, jobs, harness de teste e modo headless.

**R1 — A engine que se usa.** Editor, cena, componentes, scripting em C#, play-in-editor e save/load. *Marco: criar, scriptar e jogar.*

**R2 — Imagem.** RHI, render graph, deferred, luz, sombra, TAA, pós, áudio base e testes de imagem. *Marco: uma cena estável e bonita.*

**R3 — Mundo e produção.** Hub de projetos, editor por ferramentas, Configurações, Adicionar componente, identidade visual, asset browser, terreno, Ambiente, assets com `.cmeta`, cook incremental, compilação C# embutida, malha e material PBR, céu com IBL, CSM, TAA com velocity, névoa e build local do jogo de exemplo. *Marco: um mundo esculpido, importado, compilado e cozido.*

**R4 — Personagem.** Controller, câmera ativa, animação, skinning, IK, retarget, Tônus, cel shading e contorno do personagem. *Marco: um personagem que se move bem.*

**R5 — Cidade e streaming.** Prédios modulares, veículos, trânsito, multidão, navegação, HLOD, vegetação, água, clima, clusters de luz, PCSS, SSAO e SSR. *Marco: um quarteirão vivo e uma travessia de 1 km sem hitch.*

**R6 — UI de jogo.** Markup, layout, estilo, binding, input, animação, minimapa e editor visual. *Marco: HUD e menus de produção.*

**R7 — Conteúdo e entrega.** Importação completa, áudio de produção, profiler, pacote de produção, validação de stage e distribuição. *Marco: conteúdo em horas e jogo distribuível.*

**R8 — Fatia vertical.** Um distrito, 200 NPCs, 50 carros, 60 fps, polimento e endurecimento. *Marco: o primeiro jogo.*

## 11. Regra de mudança

Toda decisão tem ID e status:

- **FIXO** — só muda por RFC em `docs/rfcs/`, com problema, opções, custo, impacto e decisão. A mudança sobe a versão deste documento.
- **PROVISÓRIO** — muda com uma nota no histórico.

O objetivo não é congelar a engine. É impedir que uma decisão fechada seja reaberta por impulso no meio de uma release.

## 12. Histórico

- **1.4** — reescreve a D-032 por RFC-001: ícones em três camadas — Lucide 1.68.0 na UI, Fluent UI System Icons 1.1.345 nos assets e conjunto Caligo próprio para os tipos da engine.
- **1.3** — registra D-031 a D-046: editor por ferramentas, identidade de 13 px e Lucide, Ambiente, configurações em quatro seções, registro de componentes, `.cmeta`, importação determinística, glTF, automação de importação, contêiner ampliado, scripting por projeto com Roslyn embutido, build local, terreno profissional, renderer em estágios e Windows x64 como único alvo da v1; define o hub de projetos na R3 e reorganiza R3 a R7.
- **1.2** — registra a D-030: câmera como componente de cena, comportamento em script C#.
- **1.1** — confirma o limite de arquivo da D-022 como fixo, depois da R2.
- **1.0** — Tônus em módulo próprio, acima de animação e física.
- **0.9** — corpo ativo como tecnologia própria, com nome Tônus, em camadas e afinado por dados.
- **0.8** — projeto como descritor de conteúdo, com raiz, cena inicial e assembly de gameplay próprios.
- **0.7** — snapshot de play e hospedagem de script com runtime privado, contexto coletável e shadow copy.
- **0.6** — coordenadas canhotas com +X à direita, +Y para cima e +Z para frente.
- **0.5** — nlohmann/json 3.12.0 como biblioteca JSON da cena e das ferramentas.
- **0.4** — código proprietário; repositório público apenas para o CI gratuito.
- **0.3** — limite por arquivo separado em `.cpp` e `.h`, com valor provisório e allowlist; revisão após a R2.
- **0.2** — UI de jogo como sistema próprio e release R6; limite de arquivo; referência à arquitetura.
- **0.1** — fundação do escopo.
