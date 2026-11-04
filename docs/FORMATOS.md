# Caligo — Formatos

Versão 0.4

## 1. Princípio

Formato é contrato. O runtime nunca lê formato de autoria. Toda fonte é texto ou formato aberto e documentado; todo pacote é binário, versionado e independente de plataforma. Um arquivo cozido não contém caminho absoluto, ponteiro, ordem de hash instável ou carimbo de tempo.

O `.cterrain` é a exceção de representação: heightfield em texto é inviável, então ele é binário, mas o cabeçalho e os chunks são documentados neste arquivo e fazem parte do contrato. Não é formato de terceiro.

Três categorias de arquivo de origem governam o projeto:

- `.cproject` — descritor do projeto.
- `.cmeta` — identidade e configuração de importação de um asset.
- `.cscene`, `.cterrain`, `.cui` e `.cuistyle` — conteúdo de autoria.

O runtime lê apenas `.casset` e o manifesto. Nenhum parser de glTF, imagem, áudio ou cena de origem roda no jogo (D-017).

## 2. Identidade e hash

- **Asset GUID:** 128 bits aleatórios, escrito como 32 dígitos hexadecimais minúsculos. Nunca é reutilizado.
- **Sub-asset GUID:** um GUID por parte importada de uma fonte (malha, material, textura, nó de modelo). Mapeado no `.cmeta` por caminho estável.
- **EntityId:** inteiro de 64 bits, único dentro da cena. `0` é inválido e representa ausência de pai.
- **Hash de fonte e de payload:** XXH64, little-endian, escrito como 16 dígitos hexadecimais.
- **Build ID:** hash do manifesto mais versão do projeto, versão da engine e versão do compilador. Nunca inclui data ou hora.
- Referência entre assets é sempre por GUID. Referência entre entidades da mesma cena é por EntityId.
- Nome e caminho são metadados de edição; não identificam nada no runtime.

## 3. Árvore de conteúdo e projeto

```
MeuJogo/
  MeuJogo.cproject
  content/
    scenes/
    assets/
      models/
      textures/
      materials/
      audio/
      terrain/
    ui/
  scripts/
    MeuJogo.Gameplay/
  cooked/
  .caligo/
    cache/
```

- `content/` é a raiz de conteúdo.
- `cooked/` e `.caligo/cache/` são gerados e não versionados.
- Cada fonte importada tem um sidecar `.cmeta` ao lado: `coqueiro.glb` e `coqueiro.glb.cmeta`. A cena guarda o GUID no próprio arquivo e não tem sidecar; o `.cproject` também não.
- O cook é incremental: GUID, hash da fonte, hash das configurações, versão do importador e versão do cozinhador decidem se um asset precisa ser refeito.
- O runtime recebe um manifesto que mapeia GUID para arquivo cozido.

### 3.1 Descritor de projeto

`.cproject` é JSON UTF-8, versionado junto do projeto.

```json
{
  "format": "caligo.project",
  "version": 1,
  "name": "MeuJogo",
  "projectVersion": "0.1.0",
  "contentRoot": "content",
  "initialScene": "scenes/inicio.cscene",
  "gameplayProject": "scripts/MeuJogo.Gameplay/MeuJogo.Gameplay.csproj",
  "gameplayAssembly": "MeuJogo.Gameplay",
  "target": "windows-x64",
  "output": "out",
  "render": {
    "quality": "high",
    "resolutionScale": 1.0
  }
}
```

Regras:

- `version` é a versão do esquema do descritor; `projectVersion` é a versão do jogo.
- `contentRoot` é relativo à pasta do `.cproject`.
- `initialScene` é relativo a `contentRoot` e serve ao editor e ao cook. O cook resolve a cena para GUID e grava esse GUID no `build.json`; o pacote não leva o caminho de fonte.
- `gameplayProject` é relativo à pasta do `.cproject` e aponta para o projeto C#; `gameplayAssembly` é o nome do assembly gerado, não um caminho. O editor não invoca MSBuild: o `.csproj` serve ao IDE externo e a lista de fontes é o conjunto de `.cs` sob a pasta do projeto (SCRIPTING §2).
- `output` é relativo à pasta do `.cproject`; `--out` na CLI vence o campo.
- `target` só aceita `windows-x64` na v1 (D-004).
- O descritor não guarda caminho absoluto.

### 3.2 Sidecar `.cmeta`

Para `coqueiro.glb`, o sidecar é `coqueiro.glb.cmeta`. É JSON UTF-8, versionado junto da fonte.

```json
{
  "format": "caligo.meta",
  "version": 1,
  "guid": "8f14e45fceea167a5a36dedd4bea2543",
  "importer": "gltf",
  "settings": {
    "scale": 1.0,
    "generateTangents": true,
    "lodCount": 3,
    "collision": "convex"
  },
  "subAssets": [
    { "path": "Root/Body", "guid": "0f1e2d3c4b5a69788796a5b4c3d2e1f0" },
    { "path": "Root/Body/Material", "guid": "1a2b3c4d5e6f708192a3b4c5d6e7f809" }
  ]
}
```

O exemplo é de um modelo glTF. O campo `importer` seleciona o conjunto de `settings`; cada importador define os campos. Para terreno, `importer` é `terrain` e `settings` traz `materials` (GUID por camada, na ordem) e `heightmapFormat`; resolução e extensão vivem no cabeçalho do `.cterrain`, não no sidecar. O mapa `subAssets` só existe em fontes com partes.

Regras:

- O `.cmeta` guarda identidade e configuração; não guarda hash de fonte, data ou resultado de cook.
- `guid` identifica a fonte; `subAssets` identifica as partes.
- `path` é o caminho estável dentro da fonte, não o índice do array.
- GUID duplicado é erro de cook. GUID ausente é recriado com aviso; referências antigas quebram com diagnóstico.
- O `.cmeta` não contém caminho absoluto.

## 4. Cena de origem

Extensão `.cscene`, UTF-8, JSON com esquema estável. A versão atual é **4**; as versões 3, 2 e 1 continuam sendo aceitas na carga.

```json
{
  "format": "caligo.scene",
  "version": 4,
  "guid": "8f14e45fceea167a5a36dedd4bea2543",
  "environment": {
    "hours": 12.0,
    "dayLengthMinutes": 20.0,
    "weather": "clear",
    "exposure": 1.0,
    "shadows": {
      "cascades": 4,
      "distance": 200.0,
      "softness": 1.0
    },
    "fog": {
      "enabled": false,
      "height": 0.0,
      "distance": 400.0,
      "density": 0.0
    }
  },
  "render": {
    "quality": "high",
    "resolutionScale": 1.0
  },
  "entities": [
    {
      "id": 1,
      "name": "Terreno",
      "parent": 0,
      "components": [
        {
          "type": "caligo.transform",
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        {
          "type": "caligo.terrain",
          "asset": "0f1e2d3c4b5a69788796a5b4c3d2e1f0",
          "collision": true
        }
      ]
    },
    {
      "id": 2,
      "name": "Player",
      "parent": 0,
      "components": [
        {
          "type": "caligo.transform",
          "position": [0.0, 1.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        {
          "type": "caligo.script",
          "class": "Spinner"
        }
      ]
    }
  ]
}
```

O exemplo mostra os campos obrigatórios e um recorte de ambiente e render. Campo omitido recebe o padrão das tabelas 4.2 e 4.3.

Regras:

- `guid` identifica a cena; `id` identifica a entidade dentro dela.
- Todo componente tem `type` estável, nunca o nome de uma classe C++.
- Transform é obrigatório em toda entidade; ausência, duplicidade ou valor não finito é erro de carga e de cook.
- `caligo.script` guarda o nome de um tipo registrado pelo módulo de jogo. Vários scripts por entidade são permitidos; o mesmo tipo repetido é erro.
- Componentes desconhecidos nunca são descartados em silêncio: a carga falha com diagnóstico e o cook preserva os dados para uma versão que os entenda.
- A ordem das entidades não importa na origem; o cook ordena por `id`.
- Prefab é uma cena com uma raiz marcada e é instanciado no cook, não no runtime.
- A cena versão 3, que guardava o terreno embutido, continua legível. Ao salvar, o editor extrai o heightfield e os pesos para um `.cterrain`, cria o `.cmeta` com GUID e materiais padrão, avisa sobre as cores de camada da versão antiga e grava a versão 4 com o componente referenciando o asset.
- A cena versão 2, sem ambiente, é lida com ambiente padrão. A versão 3 guardava apenas `environment.hours`; os demais campos recebem o padrão. Se a versão 2 tiver terreno embutido, ela passa pela mesma extração da versão 3. Ao salvar, o editor grava a versão 4.
- A cena versão 4 não aceita `caligo.terrain` com heightfield embutido; o componente carrega apenas o GUID do `.cterrain`.
- `environment` é opcional; ausente usa os padrões da tabela 4.2. `render` é opcional; campo ausente herda o valor do projeto e valor presente na cena vence o projeto.

### 4.1 Componentes de cena

| Tipo | Multiplicidade | Dados | Entrega |
|---|---|---|---|
| `caligo.transform` | exatamente um, obrigatório | posição, rotação, escala | R1 |
| `caligo.script` | vários, mesmo tipo uma vez | classe | R1 |
| `caligo.camera` | vários, uma ativa | lente, near, far, prioridade | R4 |
| `caligo.terrain` | um por entidade; cada tile é uma entidade | GUID do `.cterrain`, colisão, viés de LOD | R3 |
| `caligo.model` | um | GUID do modelo, sobrescritas de material, sombra | R3 |
| `caligo.light` | vários | tipo, cor, intensidade, alcance, sombra | R5 |
| `caligo.collider` | vários | forma, tamanho, camada, estático | R5 |
| `caligo.vehicle` | um | massa, rodas, motor, direção, suspensão | R5 |
| `caligo.body` | um | Tônus: comportamento e nível de simulação | R4 |
| `caligo.audio` | vários | clipe, ganho, loop, atenuação | R7 |
| `caligo.prefab` | um | GUID da cena de prefab, sobrescritas | R5 |

Regras:

- `caligo.transform` não pode ser removido nem duplicado.
- `caligo.terrain` referencia um asset `.cterrain`; não carrega heightmap embutido na versão 4.
- `caligo.model` referencia o modelo cozido; materiais podem ser sobrescritos por slot.
- `caligo.light` não inclui o sol; sol e lua pertencem ao ambiente da cena.
- `caligo.camera` com `active: true` é a câmera do play; se nenhuma estiver ativa, vale a primeira em ordem de entidade (D-030).
- Componente de tipo não registrado falha a carga com diagnóstico; o cook preserva os dados.

### 4.2 Ambiente da cena

O ambiente é dado da cena, não componente de entidade. Campos:

| Campo | Padrão | Faixa |
|---|---|---|
| `hours` | 12.0 | 0 a 24 |
| `latitude` | 0.0 | -90 a 90 |
| `dayLengthMinutes` | 20.0 | 1 a 1440 |
| `weather` | `clear` | `clear`, `cloudy`, `rain`, `fog`, `storm` |
| `humidity` | 0.5 | 0 a 1 |
| `exposure` | 1.0 | 0.1 a 8.0 |
| `sky.preset` | `default` | nome de preset registrado |
| `sky.zenith` | `[0.15, 0.35, 0.65]` | cor linear |
| `sky.horizon` | `[0.65, 0.75, 0.85]` | cor linear |
| `sky.ground` | `[0.15, 0.15, 0.15]` | cor linear |
| `sky.clouds` | 0.3 | 0 a 1 |
| `sun.color` | `[1.0, 0.95, 0.85]` | cor linear |
| `sun.intensity` | 1.0 | 0 a 100 |
| `sun.angularRadius` | 0.5 | 0.1 a 10 |
| `moon.color` | `[0.65, 0.75, 1.0]` | cor linear |
| `moon.intensity` | 0.1 | 0 a 10 |
| `moon.angularRadius` | 0.5 | 0.1 a 10 |
| `shadows.cascades` | 4 | 1 a 4 |
| `shadows.distance` | 200.0 | 1 a 4000 |
| `shadows.softness` | 1.0 | 0 a 4 |
| `shadows.bias` | 0.0 | -0.01 a 0.01 |
| `fog.enabled` | `false` | booleano |
| `fog.height` | 0.0 | -1000 a 4000 |
| `fog.distance` | 400.0 | 1 a 10000 |
| `fog.density` | 0.0 | 0 a 1 |
| `fog.color` | `[0.6, 0.65, 0.7]` | cor linear |
| `wind.direction` | `[1, 0, 0]` | vetor unitário |
| `wind.strength` | 0.0 | 0 a 30 |

Cores de ambiente são lineares; a exposição do pós se aplica ao céu e à cena. O preset de céu preenche os campos de céu; o clima preenche nuvens, névoa, umidade e vento. Um campo explícito vence o preset. Presets iniciais de céu: `default`, `noon`, `dusk`, `night` e `overcast`. A direção do sol e da lua vem de `hours` e de `latitude`; `sun` e `moon` guardam aparência, não posição.

O editor pode ter pré-visualização temporária, mas o arquivo guarda apenas o valor da cena. O play pode animar a hora por script; parar restaura o snapshot (D-026).

### 4.3 Render da cena

| Campo | Padrão | Efeito |
|---|---|---|
| `render.quality` | herda o projeto | `low`, `medium`, `high` ou `ultra` |
| `render.resolutionScale` | herda o projeto | 0,5 a 2,0 |
| `render.taa` | herda o projeto | liga ou desliga a estabilização temporal |
| `render.shadowQuality` | herda o projeto | resolução e filtro da sombra |
| `render.textureBudgetMB` | herda o projeto | teto de memória de textura |

Campo ausente herda o projeto. O editor mostra o valor herdado e o valor efetivo. VSync, limite de quadros e escala de UI pertencem ao editor/usuário e não entram na cena nem no pacote.

## 5. Contêiner cozido

Extensão `.casset`. Cabeçalho de 64 bytes, little-endian.

| Offset | Tamanho | Campo |
|---|---|---|
| 0 | 4 | magic `CALG` |
| 4 | 2 | versão maior do contêiner |
| 6 | 2 | versão menor do contêiner |
| 8 | 2 | tipo |
| 10 | 2 | flags |
| 12 | 4 | tamanho do payload |
| 16 | 8 | hash da fonte |
| 24 | 16 | GUID |
| 40 | 8 | hash do payload |
| 48 | 2 | versão maior do esquema do tipo |
| 50 | 2 | versão menor do esquema do tipo |
| 52 | 4 | flags do tipo |
| 56 | 8 | reservado, zero |

O payload começa no offset 64, alinhado a 16 bytes. Payload estruturado usa FlatBuffers com esquema versionado; payload bruto usa blocos com offset e tamanho explícitos. O runtime rejeita versão maior desconhecida e aceita versão menor quando o esquema garante compatibilidade.

Tipos:

| Tipo | Nome | Entrega |
|---|---|---|
| 1 | cena | R1 |
| 2 | malha | R3 |
| 3 | textura | R3 |
| 4 | áudio | R2 |
| 5 | material | R3 |
| 6 | UI | R6 |
| 7 | fonte | R6 |
| 8 | metadados de script | R1 |
| 9 | manifesto | R3 |
| 10 | modelo | R3 |
| 11 | esqueleto | R4 |
| 12 | animação | R4 |
| 13 | colisão | R5 |
| 14 | navmesh | R5 |
| 15 | HLOD | R5 |
| 16 | terreno | R3 |

Cada tipo tem versão de esquema própria nos offsets 48 e 50. Um tipo pode evoluir sem subir a versão do contêiner; a compatibilidade é declarada por esquema.

## 6. Cena cozida

O payload de cena é organizado em tabelas paralelas, não em árvore de objetos:

- Tabela de ambiente: horas, latitude, céu, sol e lua, clima, umidade, exposição, sombras, névoa e vento.
- Tabela de render da cena: sobrescritas de qualidade, escala de resolução, TAA, sombra e orçamento de textura sobre o projeto.
- Tabela de entidades ordenada por EntityId: id, pai, intervalo de componentes, nome.
- Tabela de componentes por tipo, em arrays contíguos.
- Tabela de strings única, referenciada por offset e tamanho.
- Tabela de recursos por GUID, sem caminho de origem.
- Nenhuma referência por índice de array que dependa da ordem de autoria.

O carregamento é direto: o bloco vira memória imutável e as consultas usam os índices já ordenados.

## 7. Malha, modelo, esqueleto, animação, textura, material, áudio, terreno, colisão, navmesh e HLOD

### 7.1 Malha (tipo 2)

Uma malha cozida contém até quatro blocos de LOD. Cada bloco é independente e traz os próprios streams, índices, submalhas e limites.

Cabeçalho do bloco:

| Campo | Tipo | Observação |
|---|---|---|
| `version` | u16 | versão do bloco |
| `flags` | u16 | presença de UV1, cor, skin, morph |
| `vertexCount` | u32 | vértices do bloco |
| `indexCount` | u32 | múltiplo de 3 |
| `submeshCount` | u16 | submalhas |
| `streamCount` | u16 | streams |
| `boundsMin` | f32 × 3 | AABB |
| `boundsMax` | f32 × 3 | AABB |
| `boundingSphere` | f32 × 4 | centro e raio |

Streams:

| Stream | Formato | Obrigatório |
|---|---|---|
| `position` | f32 × 3 | sim |
| `normal` | f32 × 3 | sim |
| `tangent` | f32 × 4 | sim |
| `uv0` | f32 × 2 | sim |
| `uv1` | f32 × 2 | opcional |
| `color` | unorm8 × 4 | opcional |
| `joints` | u16 × 4 | opcional, com skin |
| `weights` | unorm16 × 4 | opcional, com skin |

Cada stream tem id, formato, stride, contagem, offset e tamanho. Os dados ficam alinhados a 16 bytes.

- Índices são `uint16` ou `uint32`, em lista de triângulos.
- Submalha tem GUID de material, offset de índice e contagem de índice.
- Skinning tem no máximo quatro influências por vértice; os pesos são normalizados e os índices apontam para o esqueleto.
- Morph targets guardam deltas de posição, normal e tangente, com nome na tabela de strings; ficam no LOD0.
- A malha é otimizada por meshopt no cook: cache de vértice, fetch e overdraw.
- LODs são gerados por meshopt, com preservação de borda e de skin.
- Coordenada: D-025, metros, canhota, +X direita, +Y cima, +Z frente. A conversão de fonte destra acontece na importação.
- NaN, infinito e triângulo degenerado são rejeitados no cook.

### 7.2 Modelo (tipo 10)

O modelo é o grafo de nós importado do glTF, já convertido.

- Tabela de nós: índice, pai, nome, transform local, GUID de malha, GUID de esqueleto.
- Um nó pode referenciar uma malha; vários nós podem referenciar a mesma malha.
- Sobrescritas de material por slot: índice do slot e GUID do material.
- Lista de animações: GUID e nome; animação padrão.
- Limites e esfera de contenção.
- O modelo é um asset de render; a entidade da cena tem um componente `caligo.model` e o transform de mundo. Os nós internos não viram entidades da cena.
- O cook não expande o modelo em entidades; prefab é outra coisa.

### 7.3 Esqueleto (tipo 11)

- Contagem de juntas, até 256.
- Índice do pai por junta; raiz = -1.
- Matriz inversa de bind, f32 × 4 × 4.
- Nome por junta na tabela de strings.
- Ordem estável, a mesma da fonte.
- Junta sem pai ou ciclo é erro de cook.

### 7.4 Animação (tipo 12)

- Clipe: nome, duração, canais.
- Canal: índice do nó, caminho (`translation`, `rotation`, `scale`, `weights`), interpolação (`step`, `linear`, `cubic`), tempos e valores.
- `translation` e `scale` usam f32 × 3; `rotation` usa quaternion f32 × 4; `weights` usa f32 × N.
- Root motion é um canal marcado; o runtime decide se aplica ao transform da entidade.
- Loop é playback, não asset.
- Vários clipes por modelo; o padrão é escolhido no componente ou no script.
- Amostragem e compressão de curva acontecem no cook; o runtime só interpola.

### 7.5 Textura (tipo 3)

- Formatos: BC1, BC3, BC5 e BC7. BC6H para HDR entra na R3.
- Flag de sRGB; cor em sRGB, dados em linear.
- Mipmaps completos até 1 × 1, com offset e tamanho por nível.
- Dimensões múltiplas de 4 para BCn.
- Normal map em BC5, dois canais; Z é reconstruído no shader.
- ORM em BC7, com R = AO, G = rugosidade e B = metal.
- Albedo e emissivo em BC1, BC3 ou BC7; alpha conforme o modo.
- Máscara e UI em BC7 ou BC3.
- O payload permite carregar mips em ordem; o streaming começa pelo nível alto.
- Formato de origem é convertido no cook, nunca no runtime.

### 7.6 Material (tipo 5)

Material PBR metallic-roughness, com extensão estilizada.

| Campo | Observação |
|---|---|
| `name` | nome de edição |
| `shadingModel` | `pbr` ou `cel` |
| `baseColorFactor` | rgba |
| `baseColorTexture` | GUID opcional |
| `metallicFactor` | 0 a 1 |
| `roughnessFactor` | 0 a 1 |
| `metallicRoughnessTexture` | GUID opcional; G = rugosidade, B = metal |
| `normalTexture` | GUID opcional e escala |
| `occlusionTexture` | GUID opcional e força |
| `emissiveFactor` | rgb |
| `emissiveTexture` | GUID opcional |
| `uvTransforms` | offset, rotação e escala por slot de textura |
| `alphaMode` | `opaque`, `mask` ou `blend` |
| `alphaCutoff` | usado em `mask` |
| `doubleSided` | booleano |
| `sampler` | endereço, filtro e anisotropia |
| `cel` | rampa, limiar de especular, rim e contorno |

Regras:

- Material é dado, não código; não há grafo de shader na v1 (D-013).
- Textura ausente em slot usado é erro de cook.
- Material referenciado por GUID; o nome não identifica.
- O contorno do cel shading é parâmetro de material; o passe de contorno está em `docs/sistemas/RENDER.md`.
- O material não guarda caminho de textura.

### 7.7 Áudio (tipo 4)

- PCM 16 bits ou float 32; taxa, canais, quadros e tamanho do bloco.
- Música longa usa streaming por blocos de 64 KB; efeito curto é carregado inteiro.
- Loop é ponto de repetição em amostras.
- Sem caminho de origem.

### 7.8 Terreno (tipo 16)

A fonte é `.cterrain`, binário versionado por tile, com sidecar `.cmeta` para GUID, materiais por camada e opções de importação. O cook grava o tipo 16.

Cabeçalho da fonte:

| Offset | Tamanho | Campo |
|---|---|---|
| 0 | 4 | magic `CTER` |
| 4 | 2 | versão maior |
| 6 | 2 | versão menor |
| 8 | 4 | tamanho do cabeçalho |
| 12 | 4 | flags |
| 16 | 4 | resolução em amostras |
| 20 | 4 | extensão em metros |
| 24 | 2 | contagem de camadas |
| 26 | 2 | contagem de chunks |
| 28 | 4 | reservado, zero |

Chunks:

| ID | Conteúdo |
|---|---|
| `HEIG` | alturas f32, resolução × resolução |
| `WGHT` | pesos u8 normalizados, resolução × resolução × camadas |
| `HOLE` | bitmask de buracos, `ceil(resolução × resolução / 8)` bytes |
| `META` | limites de altura e versão do editor, FlatBuffers |

Cada chunk tem ID de 4 bytes, tamanho u32 e payload alinhado a 4 bytes; a ordem é a da tabela.

Regras:

- Resolução de 2 a 1024 amostras; padrão 512.
- Extensão de 1 a 8192 m; padrão 512 m.
- Até 4 camadas na v1; até 8 em versão futura.
- Peso de camada é normalizado no shader; soma zero é erro.
- Altura não finita é erro.
- As alturas no `.cterrain` são f32; a importação e a exportação de heightmap usam 16 bits com escala de mundo.
- Materiais por camada vêm do `.cmeta` do tile; o cook resolve os GUIDs e grava no payload cozido.
- O tile cozido guarda heightfield, pesos, materiais, buracos, limites, colisão e metadados de LOD.
- O LOD por clipmap é gerado em runtime a partir do heightfield; HLOD distante é tipo 15.
- A colisão é heightfield; buracos removem a colisão na amostra.
- Importar e exportar heightmap de 16 bits é operação da ferramenta, não do runtime.

### 7.9 Colisão (tipo 13)

- Formas: caixa, esfera, cápsula, convexo, trimesh e heightfield.
- Cada forma tem transform local, material de física, camada e flag estático.
- Convexo é gerado por malha no cook; trimesh é usado em geometria estática; heightfield vem do terreno.
- Decomposição automática em vários convexos fica para a R5.
- O runtime não lê a malha de render para colidir; lê a forma cozida.
- Forma sem geometria válida é erro de cook.

### 7.10 Navmesh (tipo 14)

- Dado por cena, não por modelo.
- Tiles com polígonos, vértices, adjacência, links fora da malha, limites e parâmetros de agente.
- Gerado no cook a partir da geometria estática da cena.
- O formato não depende do gerador; a escolha da biblioteca de geração é decisão da R5.
- Navmesh desatualizado em relação à cena é erro de build, não aviso.

### 7.11 HLOD (tipo 15)

- Cluster por célula de streaming: limites, GUID de malha combinada, GUID de material combinado, distância de transição e lista de entidades.
- Gerado no cook a partir de geometria estática.
- O runtime escolhe entre HLOD, LOD e geometria completa por distância e cobertura de tela.
- Cluster sem malha ou material é erro de cook.

## 8. UI de jogo

- `.cui` — documento de markup, UTF-8, estrutura declarativa descrita em `docs/sistemas/UI.md`.
- `.cuistyle` — folha de estilo, UTF-8, seletores por classe, id e estado.
- O cook valida o documento, resolve fontes e sprites e gera o payload tipo 6.
- Cada nó recebe um ID estável de 64 bits derivado do caminho no documento. Mudar a hierarquia muda o ID; por isso o editor mantém IDs explícitos quando o nó precisa sobreviver a uma reorganização.
- Nenhuma string de jogo fica literal no markup: toda string é chave de localização.

## 9. Script

- Cada projeto tem o próprio assembly de gameplay: `MeuJogo.Gameplay.dll`.
- O assembly é compilado pela engine com o compilador Roslyn embutido, pelo editor ou pela CLI, e não é um asset cozido; entra no pacote em `runtime/gameplay` (D-041).
- O tipo de entrada é `MeuJogo.Gameplay.GameModule`, que implementa `Caligo.Scripting.IGameModule` e registra cada comportamento no `ScriptRegistry`.
- O assembly referencia apenas `Caligo.Scripting.dll`; não referencia render, física ou memória.
- A superfície nativa é uma ABI C versionada, definida por esquema. Os wrappers C# são escritos à mão no início e gerados quando a superfície crescer; o lado C++ nunca expõe STL, exceção ou ponteiro opaco sem contrato.
- Hospedagem framework-dependent com runtime .NET privado em `<exe>/runtime/dotnet`, bootstrap não coletável em `<exe>/runtime/host` e assemblies do jogo em `<exe>/runtime/gameplay`.
- `hostfxr` carrega o bootstrap; o bootstrap cria um `AssemblyLoadContext` coletável e faz shadow copy da pasta gerenciada antes de carregar `Caligo.Scripting.dll` e `MeuJogo.Gameplay.dll`.
- Hot reload usa o `AssemblyLoadContext` coletável. Estado estático mutável é proibido no assembly de gameplay. O contexto anterior é descarregado ao parar ou recarregar a execução.
- Sem script, a engine inicia e roda a cena. Script é uma camada, não um requisito de boot.

Detalhes em `docs/sistemas/SCRIPTING.md`.

## 10. Versionamento

- Versão maior: mudança incompatível. O runtime recusa o arquivo; o cook converte a fonte.
- Versão menor: adição compatível. O runtime ignora campos que não conhece apenas quando o esquema marca o campo como opcional.
- O contêiner tem versão própria; cada tipo tem versão de esquema própria.
- O cozinhador grava sempre a versão atual. O manifesto registra a versão de cada pacote.
- Nenhum campo é reaproveitado com outro significado. Campo removido vira reservado e permanece no esquema até a próxima versão maior.
- Cena: versão 4 atual; 3, 2 e 1 aceitas na carga, com migração ao salvar.
- Malha: bloco versão 2; a versão 1 não chegou a ser gravada em produção.
- Material, modelo, esqueleto, animação, colisão, navmesh, HLOD e terreno começam na versão 1.
- A versão do importador entra na chave do cook; mudar o importador invalida o cache.

## 11. Determinismo

- Toda gravação é little-endian e sem padding não inicializado.
- Ordem de tabelas, materiais, LODs, sub-assets, chunks e strings é estável e independente de hash de ponteiro.
- Floats são IEEE 754 de 32 bits. NaN e infinito são rejeitados no cook.
- O cook não grava data, hora, usuário, caminho absoluto ou nome da máquina.
- Dois cooks da mesma fonte, com as mesmas configurações e a mesma versão, produzem bytes idênticos.
- O build ID deriva do conteúdo, não do relógio.

## 12. Histórico

- **0.4** — projeto `.cproject`, sidecar `.cmeta`, cena versão 4 com ambiente e terreno por asset, contêiner com versão por tipo, malha v2 com UV1, skinning, submalhas e morph targets, modelo, esqueleto, animação, material PBR, terreno `.cterrain`, colisão, navmesh, HLOD e manifesto.
- **0.3** — cena na versão 2 com `caligo.script`; a carga aceita a versão 1. Hospedagem de script com runtime privado, shadow copy e contexto coletável.
- **0.2** — a carga da cena rejeita componentes desconhecidos com diagnóstico; a preservação fica no cook.
- **0.1** — formatos de cena, contêiner, malha, textura, áudio, UI e script.
