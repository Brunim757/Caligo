# Caligo — Registro de Decisões

Versão 0.9

## Como funciona

Toda decisão tem ID, status e consequência. `FIXO` só muda por RFC em `docs/rfcs/`. `PROVISÓRIO` muda com nota no histórico. O escopo define o produto; este documento preserva o raciocínio para que a decisão não seja reaberta por memória seletiva.

| ID | Status | Decisão | Motivo | Consequência |
|---|---|---|---|---|
| D-001 | FIXO | Nenhum sistema entra se não aparecer numa cena jogável. | O protótipo provou sistemas isolados e não produziu um jogo. | Toda release termina em algo visível e jogável. |
| D-002 | FIXO | Duas colunas: cidade densa e natureza estilizada. | É o campo onde a Caligo pode ser referência real. | Nenhum sistema entra sem servir a uma das duas colunas. |
| D-003 | FIXO | Núcleo em C++20; gameplay em C# sobre .NET 10 LTS. | C++ sustenta o desempenho; C# sustenta a iteração. | A fronteira é uma ABI C estável, com custo de hospedagem e marshaling. |
| D-004 | FIXO | Windows 10/11 x64 e Direct3D 11 FL11_0 na v1. | Um alvo só permite qualidade de produção. | A RHI nasce preparada para DX12, mas a v1 não entrega DX12. |
| D-005 | FIXO | Um executável com editor e runtime; a cena é a fonte única de verdade. | Elimina divergência entre autoria e jogo. | Play usa snapshot e descarta o estado ao parar. |
| D-006 | FIXO | Sem placeholder, stub ou código morto. | Código provisório vira fundação falsa. | O CI barra marcadores e o revisor barra implementação incompleta. |
| D-007 | FIXO | TDD; compilação e teste apenas no CI. | O ambiente local não é confiável para Windows. | Todo commit é validado no GitHub Actions antes de ser considerado pronto. |
| D-008 | FIXO | Todo sistema visual tem teste de imagem determinístico. | "Parece certo" não é critério. | WARP, 300 frames para sistemas temporais e imagem dourada versionada. |
| D-009 | FIXO | Sem comentários no código. | Nome e estrutura devem explicar; comentário apodrece. | O CI rejeita comentários em C++, CMake e Python. |
| D-010 | FIXO | Decisão fixa só muda por RFC escrito. | Impede reabertura por impulso no meio de uma release. | O RFC contém problema, opções, custo, impacto e decisão. |
| D-011 | FIXO | O nome é Caligo, sem sufixo. | Uma marca, um produto, sem versão no nome. | A empresa continua Kizuri Studio; a engine é Caligo. |
| D-012 | FIXO | Sem rede, VR, mobile, console ou macOS na v1. | Cada plataforma multiplica o custo de produção. | Nenhuma abstração é criada para plataformas fora do alvo. |
| D-013 | FIXO | Sem visual scripting. | O gameplay é C# e o editor não carrega duas linguagens. | Ferramentas de autoria geram C# ou dados, nunca grafos. |
| D-014 | FIXO | Sem engine genérica, marketplace ou plugins de terceiros. | A Caligo serve aos jogos do estúdio, não a um ecossistema. | Toda API é desenhada para o produto, não para adoção externa. |
| D-015 | FIXO | Sem ray tracing obrigatório e sem fotorrealismo. | O alvo é estilizado e o piso é FL11_0. | Técnicas de imagem priorizam estabilidade e custo, não física exata. |
| D-016 | FIXO | Stack da seção 5 do escopo, sem vcpkg. | Dependências fixas e reproduzíveis. | FetchContent com tag fixa; atualização de dependência é mudança deliberada. |
| D-017 | FIXO | Asset fonte em formato aberto; runtime lê apenas cozido. | Fonte diffável e pacote rápido e estável. | Nenhum parser de formato aberto roda no jogo. |
| D-018 | FIXO | Meta de 1080p60 numa GTX 1060 6 GB. | Define o orçamento real de cada sistema. | Regressão acima de 10% no orçamento quebra a release. |
| D-019 | PROVISÓRIO | Piso de hardware: FL11_0, 2 GB de VRAM, 8 GB de RAM. | Garante alcance sem sacrificar o alvo. | Pendente de confirmação da GPU de teste do estúdio. |
| D-020 | FIXO | UI de jogo separada do editor: markup, estilo em folha e binding por view-model. | UI de jogo é conteúdo; ImGui é ferramenta. | Dois sistemas, dois orçamentos, nenhuma dependência cruzada. |
| D-021 | FIXO | Limite rígido de linhas por arquivo, verificado no CI. | Arquivo monolítico é o defeito mais caro de corrigir. | Exceção só por allowlist com motivo; entrada obsoleta quebra o CI. |
| D-022 | FIXO | Limite de 600 linhas em `.cpp` e 400 em `.h`. | A R2 confirmou o valor com código real: nenhum arquivo chegou perto do limite. | Funções até 80 linhas; tipos até 20 métodos públicos. |
| D-023 | FIXO | Código proprietário; repositório público apenas para o CI gratuito. | O limite de minutos do CI privado impede o fluxo de validação. | Todo direito é reservado; visibilidade pública não concede licença. |
| D-024 | FIXO | JSON de cena e de ferramentas usa nlohmann/json 3.12.0. | Biblioteca header-only, sem dependência de toolchain extra, com tag fixa. | Parsing e gravação ficam restritos a cena e ferramentas; o runtime lê apenas o contêiner cozido. |
| D-025 | FIXO | Coordenadas canhotas, com +X à direita, +Y para cima e +Z para frente. | Direct3D 11 e DirectXMath já usam essa convenção; evita conversão por frame. | O espaço de visão tem Z positivo à frente; importadores convertem ativos de fontes destras. |
| D-026 | FIXO | Play-in-editor usa snapshot, tick fixo de 60 Hz, pausa e avanço de quadro. | A autoria precisa ficar isolada da simulação. | Parar restaura o snapshot; edição, salvamento e gizmo ficam bloqueados durante a execução. |
| D-027 | FIXO | Hospedagem de script framework-dependent com runtime .NET privado no artefato, contexto coletável e shadow copy. | O editor não pode depender do runtime da máquina nem bloquear os assemblies durante a recarga. | O artefato leva `runtime/dotnet`, `runtime/host` e `runtime/gameplay`; cada execução recria o contexto gerenciado. |
| D-028 | FIXO | Projeto é um descritor de conteúdo, não um diretório implícito: nome, raiz de conteúdo, cena inicial e assembly de gameplay próprios. | Dois jogos do estúdio não cabem na mesma árvore de conteúdo com um único assembly fixo ao lado do executável. | O editor cria, abre e alterna projetos; o runtime resolve conteúdo e gameplay pelo descritor. Primeira entrega de ferramentas depois da R1; a R1 mantém um projeto implícito. |
| D-029 | FIXO | O corpo ativo é tecnologia própria, com nome Tônus: motores que perseguem alvos de animação, equilíbrio, reflexo, cambaleio e levantar, compostos por comportamentos afináveis por dados. | O corpo que reage como gente é o que separa um mundo vivo de um mundo de bonecos, e isso exige nome, contrato e autoria como conteúdo. | Nada de middleware licenciado e nada copiado; é módulo próprio acima de animação e física, porque a pose tem um dono só; nível de comportamento por distância. Primeiro estágio junto do personagem da R4; o documento de sistema nasce na release que o implementar. |
| D-030 | FIXO | Câmera é componente de cena, com lente e seleção da câmera ativa; o comportamento de câmera é script C#. | A lente é dado de cena, versionável e editável no inspetor; o comportamento é gameplay e itera em C#. | A R4 entrega o componente e o play passa a renderizar pela câmera ativa; o viewport mantém a câmera livre do editor. |

## Histórico

- **0.9** — registra a D-030: câmera como componente de cena, comportamento em script C#.
- **0.8** — confirma o limite de arquivo da D-022 como fixo, depois da R2.
- **0.7** — corrige a fronteira do Tônus: módulo próprio acima de animação e física, porque a pose tem um dono só.
- **0.6** — registra a D-029: corpo ativo como tecnologia própria, com nome Tônus.
- **0.5** — registra a D-028: projeto como descritor de conteúdo com raiz, cena inicial e assembly próprios.
- **0.4** — registra a D-026 e a D-027: snapshot de play e hospedagem de script com runtime privado.
- **0.3** — registra a D-025: coordenadas canhotas com Z positivo à frente.
- **0.2** — registra a D-024 e a biblioteca JSON da stack.
- **0.1** — registro inicial das decisões D-001 a D-023.
