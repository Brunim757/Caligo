# Caligo — Editor

Versão 0.2

## 1. Responsabilidade

O editor é a ferramenta de produção da Caligo. Ele transforma conteúdo em cena jogável sem que o autor precise conhecer o código da engine. Não é a UI do jogo: a UI de jogo é um sistema próprio, descrito em `docs/sistemas/UI.md`; o editor usa Dear ImGui docking e ImGuizmo, como fixa a D-016.

O editor é organizado por **ferramentas**. Escolher uma ferramenta reorganiza a tela inteira para ela: o layout, o painel de parâmetros, o painel inferior e o que o viewport aceita. O viewport só faz interação 3D. Nenhum parâmetro de ferramenta mora na barra do viewport.

Essa regra vale para terreno, ambiente, animação, veículo, UI e qualquer ferramenta futura. Ela existe para impedir que o editor vire uma tela única com controles soltos.

O manual do usuário (`docs/manual/EDITOR.md`) descreve o que a release atual entrega. Este documento descreve o alvo e o contrato das ferramentas; não é promessa de que tudo já está implementado.

## 2. Princípios

- **Ferramenta primeiro.** O trabalho escolhe a ferramenta; a ferramenta escolhe o layout.
- **Viewport só 3D.** Selecionar, mover, girar, escalar, esculpir, pintar e orbitar. Nada de slider, combo ou botão de mundo no viewport.
- **Parâmetro tem dono.** Todo controle pertence a uma ferramenta, a um componente ou a Configurações. Controle órfão é defeito.
- **Densidade profissional.** Base de 13 px, espaçamento contido e ícones. O editor não parece um formulário.
- **Tudo que muda é reversível.** Toda ação que altera a cena é um comando com undo/redo; um traço de pincel é um comando.
- **Erro aparece onde acontece.** Importação, compilação, cozimento e execução mostram diagnóstico no console e no painel da ferramenta, com origem e causa.
- **Nada de ferramenta especulativa.** Uma ferramenta entra na release que tem sistema para ela (D-001).
- **O editor não é o jogo.** Nenhuma UI de jogo é desenhada com ImGui; nenhuma ferramenta do editor é funcionalidade do jogador.

## 3. Estrutura da tela

A tela tem uma moldura global e uma área de trabalho:

- **Barra de menus** — Arquivo, Editar, Exibir, Executar, Ajuda.
- **Seletor de ferramentas** — coluna vertical à esquerda, com ícone e nome; atalhos `1` a `7`.
- **Barra de estado** — FPS, memória, seleção, coordenadas, estado de compilação e importação.
- **Console** — aba inferior global, recolhível, com filtros e busca.
- **Assets** — painel global de conteúdo, disponível em qualquer ferramenta; não é uma ferramenta.
- **Área da ferramenta** — ocupa o resto: viewport, painel de parâmetros, painel contextual e abas próprias.

O layout de cada ferramenta é salvo separadamente. **Exibir → Restaurar layout** restaura o layout da ferramenta ativa, não um layout único global.

**Hub de projetos.** Quando o editor abre sem projeto, ele mostra o hub: criar, abrir, projetos recentes e alternar entre projetos. O hub é a porta de entrada da R3, quando o descritor `.cproject` e o assembly por projeto passam a existir (D-028). Com um projeto aberto, a moldura de ferramentas assume.

A execução é global, não uma ferramenta. `F5` a `F8` funcionam em qualquer ferramenta. Durante a execução, a edição de cena fica bloqueada (D-026) e o viewport mostra o mundo simulado.

## 4. Ferramentas

| Ferramenta | Atalho | Entrega | Responsabilidade |
|---|---|---|---|
| Objetos | 1 | R1/R3/R5 | entidades, hierarquia, componentes e transformação; prefabs em R5 |
| Mundo | 2 | R3/R5 | terreno; vegetação, água e streaming em R5 |
| Ambiente | 3 | R3/R5 | hora, céu, exposição, sombras e névoa; clima completo em R5 |
| Animação | 4 | R4 | clipes, blend, IK, retarget e Tônus |
| Veículo | 5 | R5 | dinâmica, suspensão e câmera |
| UI | 6 | R6 | markup, estilo, binding e pré-visualização |
| Configurações | 7 | R3 | projeto, cena, editor e render |

Ferramentas de R4 a R6 só aparecem quando o sistema existe (D-001). Até lá, o atalho não aparece ou aparece desabilitado com o motivo.

### 4.1 Objetos

A ferramenta de objetos é a base de composição da cena.

- **Hierarquia** — árvore de entidades, busca, arrastar para reparentar, criar, duplicar e excluir.
- **Viewport** — seleção, gizmo de mover/girar/escalar, espaço local ou mundo, encaixe, foco na seleção e seleção múltipla.
- **Inspetor** — nome, pai, componentes e o botão **Adicionar componente**.
- **Assets** — arrastar modelo para o viewport cria uma entidade com componente de modelo; arrastar material para o inspetor preenche um slot; arrastar script para o inspetor adiciona o componente.
- **Prefabs** — criar, instanciar e sobrescrever. Entra na R5, junto da cidade modular.

Durante o play, a ferramenta fica somente leitura e o viewport mostra o mundo simulado.

### 4.2 Mundo

A ferramenta de mundo reúne o que constrói o espaço. As abas são **Terreno**, **Vegetação**, **Água** e **Streaming**.

**Terreno.** O terreno é um heightfield por tiles, com material por camadas, não um pincel de cores.

- Criar, importar e exportar tile; o tile é um asset `.cterrain` referenciado pela cena.
- Esculpir: subir, baixar, suavizar, aplainar, terraçar, rampa, erosão e ruído.
- Pincel: raio, força, queda, espaçamento, máscara por inclinação, máscara por altura e simetria.
- Pintar: camadas de material com peso por amostra, preenchimento, amostragem e regras automáticas por inclinação e altura.
- Importar e exportar heightmap de 16 bits; importar carimbo de altura.
- LOD por clipmap, com costura entre anéis e saia contra frestas.
- Colisão por heightfield, com buracos.
- Undo por traço; pincel determinístico com semente.
- O viewport mostra o cursor do pincel, o anel de raio, o mapa de inclinação ou altura e a coloração de LOD. Os parâmetros ficam no painel da direita.

**Vegetação.** Pintar ou espalhar instâncias sobre o terreno, com densidade, LOD, vento e colisão opcional. Entra na R5, com o instancing e o HLOD. Meta: 100 mil instâncias.

**Água.** Plano e rio, com material, reflexo e espuma. Entra na R5, junto da cidade e das superfícies molhadas.

**Streaming.** Células, orçamento de memória, distância de visão e HLOD. Entra na R5. O mundo é dividido em células; carregar e descarregar não pode gerar hitch acima do orçamento.

### 4.3 Ambiente

A ferramenta de ambiente é a dona do mundo visível. Ela substitui o controle de hora que hoje vive na barra do viewport.

- Hora do dia, duração do dia e ciclo em play.
- Sol e lua: cor, intensidade e raio angular; a direção vem da hora e da latitude.
- Céu: preset, zênite, horizonte, chão e nuvens.
- Clima: limpo, nublado, chuva, névoa e tempestade; umidade e vento.
- Exposição, contraste e saturação de pré-visualização.
- Sombras: cascatas, distância, suavidade e viés.
- Névoa: altura, distância, cor e densidade.
- Presets de ambiente e transições por script.

Os dados de ambiente pertencem à cena. O editor pode ter uma pré-visualização temporária, mas ela não é gravada e não entra no pacote. O play pode animar a hora por script; parar restaura o snapshot (D-026).

### 4.4 Animação e Tônus

A ferramenta de animação entra na R4, com o personagem.

- Clipes, linha do tempo, blend 1D/2D, layers, IK de membros, retarget e root motion.
- Tônus: equilíbrio, reflexo, cambaleio, agarrar e levantar, com nível de simulação por distância (D-029).
- A pré-visualização roda no viewport; os parâmetros ficam no painel da ferramenta.

### 4.5 Veículo

Entra na R5. Dinâmica, suspensão, câmera e ajuste de dirigibilidade. O veículo é uma entidade com componentes próprios; a ferramenta edita os parâmetros e testa no viewport.

### 4.6 UI

Entra na R6. Edita markup `.cui`, estilo `.cuistyle` e binding para view-models em C#, com pré-visualização ao vivo. A UI de jogo não compartilha widgets com o editor (D-020).

### 4.7 Configurações

A ferramenta de configurações é uma tela completa, não um popup. Ela tem quatro seções e uma regra de precedência.

**Projeto.** Nome, versão, raiz de conteúdo, cena inicial, projeto de gameplay, assembly, alvo, saída de build e padrões de render. Gravado em `MeuJogo.cproject`.

**Cena.** Ambiente e sobrescritas de render. Gravado em `.cscene`. Gravidade, navegação e streaming entram com a física e a cidade (R5).

**Editor.** Tema, escala de UI, tamanho de ícone, idioma, salvamento automático, compilar ao salvar, caminho do IDE, VSync, limite de quadros, visões de depuração, velocidade da câmera, grade, encaixe, filtros do console e layout por ferramenta. Gravado em `%APPDATA%/Caligo/editor.json`.

**Render.** Preset de qualidade, escala de resolução, TAA, cascatas, qualidade de sombra e orçamento de textura. Gravado em `render` no `.cproject`, com sobrescrita em `.cscene`; a pré-visualização do editor não é gravada.

Precedência:

1. Padrão da engine.
2. Projeto.
3. Cena.
4. Pré-visualização do editor, apenas em memória.

O build lê **projeto e cena**. Configuração de usuário e pré-visualização do editor nunca entram no pacote.

### 4.8 Adicionar componente

O inspetor tem um botão **Adicionar componente** no fim da lista. Ele abre uma busca com categorias: Básico, Render, Física, Scripts, Áudio e Mundo.

Cada componente é um tipo registrado com nome estável `caligo.*`. O registro declara nome de exibição, categoria, ícone, multiplicidade, obrigatoriedade e fábrica. A cena adiciona, remove e consulta por tipo; a UI não conhece os componentes individualmente.

O registro cobre os tipos da v1:

| Tipo | Multiplicidade | Entrega |
|---|---|---|
| `caligo.transform` | exatamente um, obrigatório | R1 |
| `caligo.script` | vários, mesmo tipo uma vez | R1 |
| `caligo.camera` | vários, uma ativa | R4 |
| `caligo.terrain` | um por entidade; cada tile é uma entidade | R3 |
| `caligo.model` | um por entidade | R3 |
| `caligo.light` | vários | R5 |
| `caligo.collider` | vários | R5 |
| `caligo.vehicle` | um | R5 |
| `caligo.body` | um | R4 |
| `caligo.audio` | vários | R7 |
| `caligo.prefab` | um | R5 |

Regras:

- `caligo.transform` não pode ser removido nem duplicado.
- Scripts vêm do assembly do projeto; a lista é atualizada depois de cada compilação.
- O mesmo tipo de script não se repete na mesma entidade.
- Componente desconhecido falha a carga com diagnóstico (FORMATOS §4); o cook preserva os dados para uma versão que os entenda. O editor nunca descarta o componente em silêncio.
- Adicionar e remover componente são comandos de undo.

## 5. Identidade visual

A identidade visual é parte da ferramenta, não enfeite. Densidade e legibilidade andam juntas; o padrão é 13 px.

### 5.1 Tipografia

- Fonte de UI: 13 px lógicos, Segoe UI no Windows, com fallback para a fonte padrão do ImGui.
- Fonte do console: 13 px monoespaçada, Consolas no Windows, com fallback.
- Escala do usuário: 100%, 110% ou 125%, aplicada sobre a base.
- DPI do sistema continua aplicado por `Pixels()`; a escala do usuário é multiplicada depois.
- Nada de 15,5 px ou 17 px como padrão.

### 5.2 Métricas

| Item | Valor |
|---|---|
| Padding de janela | 8 × 8 |
| Padding de frame | 6 × 4 |
| Espaçamento entre itens | 6 × 4 |
| Espaço interno | 4 × 3 |
| Padding de célula | 4 × 3 |
| Indentação | 14 |
| Barra de rolagem | 12 |
| Altura de linha | 22 |
| Raio de canto | 4 |
| Borda | 1 |

As métricas são multiplicadas pelo DPI, como já faz `ApplyTheme`.

### 5.3 Cores

Tema escuro neutro, com um acento azul. Tokens fixos:

| Token | Valor |
|---|---|
| Fundo de janela | `#181A1F` |
| Fundo de painel | `#15171B` |
| Fundo de campo | `#262A31` |
| Borda | `#2D3139` |
| Texto | `#E8EBF0` |
| Texto secundário | `#828894` |
| Acento | `#00A2FF` |
| Acento em hover | `#46BEFF` |
| Seleção | `#1A68A4` |
| Erro | `#E5534B` |
| Aviso | `#D9A441` |
| Sucesso | `#4CAF7D` |

O acento marca foco, seleção e ação primária. Não é usado em tudo.

### 5.4 Ícones

Os ícones cobrem dois trabalhos visuais distintos, em três camadas:

- **Ação** — toolbar, botões, menus e gizmo usam **Lucide 1.68.0** (ISC), contorno, grade 24 × 24 e traço 2; rasterizado em 16, 20 e 24 px, com 16 px de padrão.
- **Tipo** — asset browser, inspetor, abas, hierarquia e arrasto usam **Fluent UI System Icons 1.1.345** (MIT), regular, filled e color, nos tamanhos de 16 a 48.
- **Tipo da engine** — os tipos que biblioteca nenhuma cobre usam o conjunto **Caligo próprio**, preenchido e de grade 24 × 24: `project` (`.cproject`), `meta` (`.cmeta`), `scene` (`.cscene`), `terrain` (`.cterrain`), `cooked` (`.casset`), `ui` (`.cui`) e `ui-style` (`.cuistyle`).

Regras:

- Cada ícone tem código semântico estável em manifesto versionado. O código é o contrato; o glifo é a implementação e troca sem tocar em dado, cena ou código.
- O ícone de tipo pode usar a variante colorida. A cor de estado, com os tokens de erro, aviso e sucesso, é sobreposta e nunca substitui o tipo.
- Botão só de ícone tem tooltip. Ação primária usa ícone e texto.
- Nenhum ícone é emoji ou caractere Unicode avulso.
- Os avisos de licença do Lucide, do Fluent e do conjunto próprio acompanham o pacote da engine.

Ícones da primeira leva: novo, abrir, salvar, desfazer, refazer, executar, pausar, passo, parar, compilar, configurações, buscar, adicionar, remover, duplicar, pasta, pasta aberta, arquivo, projeto, meta, cozido, cena, script, projeto de script, material, textura, malha, terreno, céu, luz, câmera, modelo, colisor, áudio, UI, estilo, documento, aviso, erro, informação, check, chevron, olho, cadeado, estrela, grade, encaixe, mover, girar, escalar, pincel, pintar, apagar, importar, exportar, build, pacote, console, hierarquia, inspetor, assets e profiler.

O navegador de assets usa as camadas de tipo; o mapa de código para ícone está em `docs/sistemas/ASSETS.md` §11.1.

## 6. Undo, seleção e comandos

- O histórico é por cena, com limite de 200 comandos.
- Um arrasto de transformação é um comando; um traço de pincel é um comando; um preenchimento de camada é um comando.
- Adicionar, remover, renomear, reparentar, criar e excluir entidade são comandos.
- Alterar ambiente e configurações de cena é comando; alterar configuração de usuário não é.
- O play bloqueia o histórico. Parar restaura o snapshot e o histórico continua válido.
- Abrir outra cena limpa o histórico.
- Atalhos: `Ctrl+Z`, `Ctrl+Y` e `Ctrl+Shift+Z`.
- Seleção múltipla usa `Ctrl` e `Shift`; o inspetor edita o valor comum e mostra o que diverge.

## 7. O que entra em cada release

| Release | Editor |
|---|---|
| R3 | hub de projetos; moldura de ferramentas, Objetos com adicionar componente, undo/redo e multi-seleção; Mundo com terreno; Ambiente; Configurações; asset browser; identidade visual de 13 px e ícones |
| R4 | Animação e Tônus; componente de câmera ativa |
| R5 | Veículo; prefabs; colisão, navegação, vegetação, água e streaming no Mundo |
| R6 | UI de jogo com pré-visualização |
| R7 | profiler, ferramenta de áudio, polimento e produção |

A ordem respeita a D-001: a ferramenta entra com o sistema que ela edita.

## 8. Fora de escopo

- UI própria do zero; a stack é ImGui + ImGuizmo (D-016).
- Editor de código dentro da engine.
- Visual scripting (D-013).
- Marketplace, plugins e temas de terceiros (D-014).
- Edição colaborativa em rede.
- Editor em outra plataforma que não Windows x64 na v1 (D-004, D-012).

## 9. Histórico

- **0.2** — reescreve §5.4 por RFC-001: ícones em três camadas — Lucide na UI, Fluent UI System Icons nos assets e conjunto Caligo próprio para os tipos da engine —, no lugar de "Lucide para tudo".
- **0.1** — define o hub de projetos, o editor por ferramentas, o viewport só de interação 3D, a ferramenta de terreno, a de ambiente, as configurações em quatro seções, o registro de componentes, a identidade visual de 13 px e os ícones Lucide.
