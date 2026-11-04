# Caligo — Assets e Importação

Versão 0.2

## 1. Responsabilidade

O sistema de assets transforma arquivos de origem em dados cozidos que o runtime carrega. Ele cobre identidade, configurações de importação, validação, dependências, cozimento, manifesto, cache e o navegador de assets do editor.

O princípio é a D-017: fonte em formato aberto, runtime lê apenas cozido. Nenhum parser de glTF, imagem ou áudio roda no jogo. O jogo recebe GUIDs e bytes cozidos.

O sistema não é um marketplace e não aceita plugins (D-014). A superfície de importação é fechada e versionada.

## 2. Princípios

- **Identidade antes de caminho.** Cena, material e prefab referenciam GUID, nunca nome de arquivo.
- **Importar é compilar.** Fonte + configurações + versão do importador produzem bytes idênticos.
- **Erro é do cook.** Geometria inválida, textura ausente ou extensão não suportada falham na importação, com nó e causa; nunca em play.
- **Automático com override.** LOD, tangentes, UV de lightmap, colisão e compressão nascem prontos e podem ser sobrescritos por asset.
- **Nada de mágica silenciosa.** Toda decisão automática é registrada no diagnóstico e visível no painel de importação.
- **Cache é descartável.** Apagar o cache e cozinhar de novo produz o mesmo resultado.
- **O pacote não carrega fonte.** Nem `.gltf`, nem `.png`, nem `.cs`; apenas `.casset` e o manifesto.

## 3. Projeto e árvore de conteúdo

O projeto é um descritor (D-028), não um diretório implícito. A árvore padrão:

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

- `content/` é a raiz de conteúdo declarada no `.cproject`.
- `scripts/` fica fora da raiz de conteúdo; script é código, não asset.
- `cooked/` e `.caligo/cache/` são gerados e não versionados.
- O repositório da engine ignora `cooked/` e `.caligo/cache/`; o projeto de um jogo faz o mesmo.
- O contrato completo do `.cproject` está em `docs/FORMATOS.md` §3.1.
- Toda fonte importada tem um sidecar `.cmeta` ao lado: `coqueiro.glb` e `coqueiro.glb.cmeta`. A cena é a exceção: o `.cscene` carrega o GUID no próprio cabeçalho e não tem sidecar. Terreno usa `.cmeta` para identidade, materiais por camada e opções de heightmap; UI e estilo usam `.cmeta` apenas para identidade.

## 4. Identidade

### 4.1 GUID

- 128 bits aleatórios, 32 dígitos hexadecimais minúsculos, nunca reutilizados.
- Criado no primeiro import e gravado no sidecar `.cmeta` ao lado da fonte; a cena já nasce com o GUID no arquivo.
- Mover ou renomear o arquivo não muda o GUID, desde que o sidecar acompanhe.
- GUID duplicado é erro de cook. GUID ausente é recriado com aviso; referências antigas quebram e o diagnóstico aponta a cena que as usa.
- Excluir a fonte não libera o GUID. Cenas que ainda o referenciam falham com diagnóstico, não com entidade vazia.

### 4.2 Sidecar `.cmeta`

Para `coqueiro.glb`, o sidecar é `coqueiro.glb.cmeta`. É JSON, UTF-8, versionado junto da fonte.

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

O exemplo é de um modelo glTF. O campo `importer` seleciona o conjunto de `settings`; cada importador define os campos. Para terreno, `importer` é `terrain` e `settings` traz `materials` (GUID por camada, na ordem) e `heightmapFormat`. O mapa `subAssets` só existe em fontes com partes.

- O sidecar guarda identidade e configuração; não guarda hash de fonte nem resultado de cook. Isso é cache.
- O sidecar é a fonte de verdade da identidade dos assets importados. Sem ele, o asset perde a identidade; a cena não tem sidecar e guarda o GUID no próprio arquivo.
- O sidecar não contém caminho absoluto.

### 4.3 Sub-assets

Um modelo, uma cena glTF ou uma pasta de UI pode conter vários assets. Cada sub-asset tem GUID próprio, mapeado no `.cmeta` por caminho estável dentro da fonte.

- Caminho estável é o caminho de nós do glTF, com nomes e índices, não o índice do array.
- Renomear um nó mantém o GUID pelo mapa; se o caminho não existir mais, o sub-asset vira órfão com aviso.
- Nó novo recebe GUID novo.
- Caminho ambíguo, com dois nós iguais no mesmo nível, falha com diagnóstico e exige renomear na fonte.
- Material e textura embutidos no glTF também recebem sub-asset; material externo referenciado por GUID próprio.

## 5. Formatos de origem

| Tipo | Formatos aceitos | Observação |
|---|---|---|
| Modelo | glTF 2.0: `.glb` ou `.gltf` + `.bin` + imagens | cgltf; conversão de eixo na importação (D-025) |
| Textura | PNG, TGA, JPEG, BMP, HDR (Radiance) | stb; sem EXR na v1 |
| Áudio | WAV, FLAC, MP3 | miniaudio; PCM no cozido |
| Cena | `.cscene` | JSON versionado |
| UI | `.cui`, `.cuistyle` | markup e estilo |
| Terreno | `.cterrain` | heightfield por tile |
| Script | `.cs` | compilado pela engine (editor ou CLI), não cozido como asset |

Regras:

- **FBX não entra na v1.** É formato proprietário e contraria a D-017. O diagnóstico orienta exportar glTF do DCC.
- **Draco e BasisU não entram.** Foram cortados da stack; um glTF que os exija falha com diagnóstico claro.
- **EXR não entra na v1.** HDR Radiance entra; EXR fica para quando a stack tiver um decodificador.
- **Extensões glTF têm lista fechada.** Aceitas na v1: `KHR_materials_emissive_strength`, `KHR_texture_transform` e `KHR_mesh_quantization`. Extensão obrigatória desconhecida falha; extensão opcional desconhecida gera aviso e é ignorada.
- Extensão aceita é resolvida na importação: `KHR_materials_emissive_strength` multiplica o fator emissivo, `KHR_texture_transform` vira `uvTransforms` no material e `KHR_mesh_quantization` é desquantizado para os streams f32.
- **GLB é o formato recomendado** para modelo com muitas dependências; `.gltf` solto também é aceito e cada arquivo referenciado vira dependência.
- **Unidade é metro.** glTF já é metro; escala de importação é multiplicador, com padrão 1.
- **Eixo é convertido uma vez.** A cena e o runtime usam a convenção da D-025: canhota, +X direita, +Y cima, +Z frente.

## 6. Configurações de importação

As configurações vivem no `.cmeta`, têm padrão por tipo e são sobrescrevíveis por asset. O hash das configurações entra na chave do cook.

### 6.1 Modelo

| Configuração | Padrão | Efeito |
|---|---|---|
| Escala | 1,0 | multiplicador de unidade |
| Eixo de origem | automático | corrige exportação com up axis errado |
| Normais | importar; gerar se faltar | suavização por ângulo quando gera |
| Tangentes | gerar | MikkTSpace |
| Cores de vértice | importar | usadas pelo material quando existirem |
| Materiais | importar | PBR metallic-roughness para material da Caligo |
| LODs | 3 | até 4; simplificação por meshopt |
| Colisão | caixa | caixa, convexo, trimesh ou nenhuma |
| UV de lightmap | gerar | xatlas; exige UV1 |
| Animações | todas | filtro por nome; R4 |
| Root motion | detectar | R4 |
| Morph targets | importar | R4 |
| Influências por vértice | 4 | limite do formato |
| Otimização | cache e overdraw | meshopt |

### 6.2 Textura

| Configuração | Padrão | Efeito |
|---|---|---|
| Tipo | albedo | albedo, normal, rough/metal/AO, emissivo, máscara, UI |
| sRGB | conforme o tipo | cor em sRGB; dados em linear |
| Compressão | automática | BC1, BC3, BC5 ou BC7 |
| Mips | completos | geração até 1 × 1 |
| Tamanho máximo | 2048 | reduz na importação |
| Endereço | repetir | repetir, limitar, borda |
| Filtro | linear + mips | linear, ponto, anisotrópico |
| Canal | automático | ORM empacotado ou mapas separados |
| Normal | DirectX | flip do canal verde quando a fonte for OpenGL |
| Alpha | opaco | opaco, máscara, blend |

Regras de compressão:

- Albedo e emissivo: BC1 para opaco, BC3 para alpha suave, BC7 para qualidade alta.
- Normal: BC5, dois canais, Z reconstruído no shader.
- ORM: BC7, com R = AO, G = rugosidade e B = metal.
- Máscara e UI: BC7 ou BC3, conforme alpha.
- Dimensões precisam ser múltiplas de 4 para BCn.
- HDR: BC6H quando a fonte for HDR; entra na R3.

### 6.3 Terreno

O `.cterrain` guarda extensão, resolução, camadas, alturas, pesos e buracos. O `.cmeta` guarda o GUID, `materials` (GUID por camada) e `heightmapFormat`. A cena guarda colisão e viés de LOD no componente.

| Configuração | Onde vive | Padrão | Efeito |
|---|---|---|---|
| Extensão do tile | `.cterrain` | 512 m | tamanho no mundo |
| Resolução | `.cterrain` | 512 amostras | 1 m de espaçamento no padrão |
| Camadas | `.cterrain` | 4 | até 8 em versão futura |
| Materiais | `.cmeta` | obrigatórios | GUID por camada, na ordem |
| Colisão | cena | heightfield | liga ou desliga por tile |
| Buracos | `.cterrain` | desligado | bitmask por amostra |
| Heightmap | `.cmeta` | importar/exportar 16 bits | PNG ou RAW com escala de mundo |
| LOD | cena | clipmap | anéis e costura |

### 6.4 Áudio

| Configuração | Padrão | Efeito |
|---|---|---|
| Modo | memória | memória para efeito, streaming para música |
| Loop | desligado | ponto de loop |
| Taxa | manter | converte no cook se necessário |
| Canais | manter | mono ou estéreo |
| Formato cozido | PCM 16 bits | float 32 para música |

## 7. Validação e diagnóstico

O cook valida antes de gravar. Erro bloqueia o asset; aviso permite cozinhar e aparece no painel.

**Modelo**

- Posição, normal, tangente e peso não finitos: erro.
- Triângulo degenerado ou área zero: erro.
- UV0 ausente com material que usa textura: erro.
- Normal ausente: gera e avisa.
- Escala negativa: corrige winding e avisa.
- Nó com ciclo, hierarquia profunda demais ou influência acima de 4: erro.
- Textura referenciada e ausente: erro.
- Extensão obrigatória desconhecida: erro.

**Textura**

- Dimensão não múltipla de 4 para BCn: erro.
- Dimensão acima do máximo: reduz e avisa.
- Normal map com alfa: avisa e descarta alfa.
- SRGB em textura de dados: erro.

**Terreno**

- Altura não finita: erro.
- Peso de camada fora de 0 a 1 ou soma zero: erro.
- Material de camada ausente: erro.
- Resolução ou extensão fora da faixa: erro.
- Buraco fora dos limites: erro.

O diagnóstico tem código estável, severidade, asset, nó ou amostra e mensagem. O console do editor mostra; o build falha se houver erro.

## 8. Cozimento

### 8.1 Chave

A chave de um asset é composta por:

- hash da fonte e das dependências de origem (`.bin`, imagens);
- hash do `.cmeta`, incluindo configurações e mapa de sub-assets;
- versão do importador;
- versão do cozinhador;
- alvo (`windows-x64`).

Mudou qualquer item, o asset recoze. Nada mais entra na chave.

### 8.2 Cache e dependências

- O cache fica em `.caligo/cache/` e é gerado; apagar não perde fonte.
- O cache guarda chave, saída, hash, tamanho e grafo de dependências.
- Dependência aponta do consumidor para o produtor: material → textura; modelo → material; cena → modelo.
- Mudar uma textura invalida o material que a usa, o modelo que usa o material e a cena que usa o modelo.
- O cook é paralelo por jobs, mas a saída é ordenada e determinística; a ordem de conclusão não altera bytes.

### 8.3 Manifesto

- O cook grava `cooked/manifest.casset`, tipo 9.
- O manifesto mapeia GUID para arquivo, tipo, hash, tamanho, versão e dependências.
- O manifesto é o único índice que o runtime lê; o runtime não varre pasta.
- Um asset com erro não entra no manifesto; o build falha e lista todos os erros.
- O build ID deriva do hash do manifesto e dos metadados de build, nunca do relógio.

### 8.4 Determinismo

- Mesma fonte, mesmas configurações e mesma versão produzem bytes idênticos.
- Sem data, hora, usuário, caminho absoluto, nome de máquina ou ordem de hash de ponteiro.
- Floats são IEEE 754 de 32 bits; NaN e infinito são rejeitados.
- Ordem de tabelas, materiais, LODs, sub-assets e strings é estável.

## 9. Tipos cozidos

O contêiner e os payloads estão em `docs/FORMATOS.md` §5 e §7.

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

O assembly de gameplay não é um asset cozido. Ele é compilado pela engine, pelo editor ou pela CLI, e entra no pacote em `runtime/gameplay`, como descreve `docs/sistemas/SCRIPTING.md`.

## 10. Runtime

- O runtime abre o manifesto, valida magic, versão e hash e resolve GUID para arquivo.
- Carregamento é assíncrono, por jobs de IO; a cena não bloqueia a thread principal.
- Recurso ausente ou corrompido vira erro com GUID e arquivo; não vira objeto vazio.
- Textura e malha têm orçamento de memória e streaming por distância e tela.
- Cena, terreno e HLOD carregam por célula; descarregar não pode gerar hitch acima do orçamento.
- Reimportar no editor atualiza o cozido e recarrega textura e material no play seguinte. Hot reload de assets durante o play é completado na R7.

## 11. Navegador de assets

O navegador é um painel global, disponível em qualquer ferramenta. Ele não é uma ferramenta.

- Árvore por pasta, busca, filtro por tipo e breadcrumb.
- Importar por arrastar arquivo para a janela ou por comando.
- Fila de importação com progresso, cancelamento e erro por asset.
- Selecionar asset mostra o `.cmeta` e o painel de configurações de importação; na cena, mostra o cabeçalho com GUID, versão e ambiente.
- Reimportar, localizar a fonte no Explorer e abrir no aplicativo padrão.
- Criar pasta, cena, script, material e terreno.
- Arrastar modelo para o viewport ou hierarquia; material para o inspetor; script para o inspetor.
- Miniatura de modelo, textura e material entra na R7.
- Excluir e mover atualizam o sidecar, quando houver; mover mantém o GUID. A cena guarda o GUID no próprio arquivo.

### 11.1 Ícones

Toda linha do navegador tem ícone. Não existe linha só de texto, e o mesmo ícone do tipo aparece no inspetor, nas abas, na hierarquia e no arrasto.

- Pasta tem dois estados: fechada e aberta; o aberto acompanha a expansão.
- O ícone de tipo usa a camada **Fluent UI System Icons 1.1.345** (MIT); o tipo pode usar a variante regular, filled ou color. A cor de estado, com os tokens de erro, aviso e sucesso, é sobreposta e nunca substitui o tipo.
- Os tipos que biblioteca nenhuma cobre usam o conjunto **Caligo próprio**, preenchido.
- O mapa `código → glifo` é dado versionado; tipo novo entra no mapa, não no código da UI.
- O nome acompanha o ícone. Ícone sozinho só em modo compacto, com tooltip.
- O código semântico é o contrato e o glifo é a implementação: trocar a camada ou o desenho não muda dado nem código.

O glifo Fluent é o nome-base, sem sufixo de tamanho ou estilo (`folder`, não `folder_24_regular`).

Nas linguagens, há duas formas: o **documento marcado** (`document_cs`) é o arquivo na linha do navegador; a **marca solta** (`code_cs`) é o componente ou a categoria de script no inspetor. Os documentos marcados por linguagem existem só em 16 px, que é o padrão do navegador.

| Entrada | Código | Ícone |
|---|---|---|
| Pasta fechada / aberta | `folder` / `folder-open` | Fluent `folder` / `folder_open` |
| Projeto `.cproject` | `project` | Caligo `project` |
| Cena `.cscene` | `scene` | Caligo `scene` |
| Sidecar `.cmeta` | `meta` | Caligo `meta` |
| Terreno `.cterrain` | `terrain` | Caligo `terrain` |
| UI `.cui` / estilo `.cuistyle` | `ui` / `ui-style` | Caligo `ui` / `ui-style` |
| Modelo `.glb`, `.gltf` | `model` | Fluent `cube` |
| Textura | `texture` | Fluent `image` |
| Material | `material` | Fluent `color_fill` |
| Áudio | `audio` | Fluent `music_note_2` |
| Script `.cs` | `script` | Fluent `document_cs` |
| Projeto C# `.csproj` | `script-project` | Fluent `document_code` |
| Cozido `.casset` | `cooked` | Caligo `cooked` |
| Manifesto `.casset` tipo 9 | `manifest` | Fluent `clipboard` |
| `build.json` | `build` | Fluent `data_usage` |
| Pacote `.zip` | `package-file` | Fluent `folder_zip` |
| Documento `.md`, `.txt` | `document` | Fluent `document_text` |
| Documento `.json` | `document-data` | Fluent `braces` |
| Desconhecido | `file` | Fluent `document` |

Sobreposto ao ícone do tipo, um estado discreto:

| Estado | Sinal |
|---|---|
| Cozido e atual | check no canto |
| Pendente de importação | ponto |
| Desatualizado: fonte ou configuração mudou | seta circular |
| Erro de importação ou cook | aviso na cor de erro |
| Sem `.cmeta` | interrogação, até o primeiro import |
| Fora do projeto ou somente leitura | cadeado |
| Favorito | estrela |
| Selecionado | realce na cor de seleção |

O `.cproject` e os arquivos da própria engine também têm ícone. Nenhum arquivo do projeto aparece sem ícone.

As três camadas de ícone — Lucide na UI, Fluent nos assets e conjunto Caligo próprio — estão fixadas na D-032 e detalhadas no RFC-001 (`docs/rfcs/RFC-001-icones-do-editor.md`).

## 12. Provas

- Cozinhar duas vezes a mesma fonte produz o mesmo hash de manifesto e os mesmos bytes.
- Um modelo de referência com malha, material, textura e LOD cozinha e renderiza em WARP com imagem dourada.
- Um modelo com triângulo degenerado, textura ausente e extensão Draco falha com diagnóstico específico.
- Mudar uma textura invalida o material, o modelo e a cena dependentes, e só eles.
- O manifesto resolve todos os GUIDs e nenhum caminho de fonte aparece no pacote.
- Um asset removido deixa referência quebrada visível, não silenciosa.

## 13. O que entra em cada release

| Release | Assets |
|---|---|
| R3 | identidade `.cmeta`, glTF estático, textura, material, terreno, cook incremental, manifesto, asset browser, LOD, tangentes e UV1 básicos, colisão de terreno, provas de determinismo |
| R4 | esqueleto, animação, morph targets e skinning |
| R5 | colisão composta, navmesh, HLOD, streaming de textura e malha |
| R7 | ferramenta completa de importação, miniatura, reimport em massa, áudio e UI de produção, hot reload de assets |

## 14. Fora de escopo na v1

- FBX, USD, Draco, BasisU e EXR.
- Auto-rigging fora da v1; retarget de animação entra na R4.
- Marketplace, loja e plugins (D-014).
- Importação em nuvem.
- Editor de material por grafo (D-013).
- Cozimento para plataforma que não Windows x64 (D-004, D-012).

## 15. Histórico

- **0.2** — reescreve §11.1 por RFC-001: o mapa de ícones passa a `código → glifo Fluent`, mais o conjunto Caligo próprio para os tipos da engine, no lugar de "Lucide para tudo".
- **0.1** — define identidade `.cmeta`, importação glTF, configurações por asset, validação, cozimento determinístico, dependências, manifesto, navegador com mapa de ícones por tipo e o recorte de releases.
