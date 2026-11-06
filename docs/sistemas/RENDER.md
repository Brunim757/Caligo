# Caligo — Renderização

Versão 0.2

## 1. Responsabilidade

O sistema de renderização transforma a cena em imagem. Ele cobre a camada de recurso gráfico, o grafo que ordena os passes e o pipeline diferido com geometria, material, luz, sombra, estabilização temporal e pós-processamento. O editor e o runtime usam o mesmo renderizador; não há um caminho de desenho para a ferramenta e outro para o jogo.

A v1 desenha sobre Direct3D 11 em FL11_0, alvo fixo da release. A camada de recurso gráfico nasce pequena e explícita, preparada para DX12 sem prometer DX12. Fora do alvo: ray tracing e fotorrealismo. A prioridade é estabilidade e custo previsível.

O renderizador é determinístico sob WARP, o que permite validar a imagem no CI sem GPU dedicada.

Este documento separa o que a R2 entrega do que é alvo das release seguintes. A R2 é a fundação; ela não é o renderer final.

## 2. Camada de recurso gráfico

Dois objetos se dividem a fronteira com a API:

- **Dispositivo** — cria o device Direct3D 11, o contexto, e resolve o adaptador. Pode ser forçado a WARP para teste; expõe o nome do driver e se está em WARP.
- **Dispositivo gráfico** — cuida da swapchain e do present: criação, redimensionamento, início e fim de quadro. O desenho da cena não conhece a janela.

Recursos:

- **Buffer** — vértice, índice ou constante.
- **Textura** — alvo de render, profundidade ou amostrável.
- **Shader** — par de vértice e pixel, compilado em runtime com `D3DCompile` a partir de HLSL embutido; não há etapa offline de compilação de shader.
- **Layout de entrada** — liga o fluxo de vértice aos semânticos do shader.
- **Pipeline** — estado fixo: descarte de face, escrita de profundidade e formato.
- **Amostrador** — filtro, modo de endereço, comparação e borda.

Formatos:

| Formato | Uso |
|---|---|
| `Rgba8Unorm` | imagem final apresentada |
| `Rgba8UnormSrgb` | albedo do G-buffer, quando o material usa cor sRGB |
| `Rgba16Float` | HDR linear, normal do G-buffer, histórico temporal |
| `Rg16Float` | velocity buffer |
| `Depth32Float` | profundidade do G-buffer e mapa de sombra |

## 3. Grafo de renderização

O quadro é declarado como passes antes de qualquer submissão. O grafo cuida do ciclo de vida dos recursos:

- Passes executam na ordem de declaração.
- `Read` e `Write` registram o uso; ler um recurso antes de ele ser escrito é recusado com erro, não silenciado.
- Recursos transientes são alocados internamente e têm memória reaproveitada entre vidas que não se cruzam.
- Texturas externas entram por importação e o grafo resolve o mesmo identificador para quem escreve e para quem lê.
- Antes de cada passe, os oito slots de recurso de shader são desligados, para que nenhum passe enxergue a ligação do anterior.

Um passe recebe apenas o contexto do grafo, de onde tira o dispositivo e as texturas por identificador. Passes sem recurso também são válidos — a sombra é declarada assim.

## 4. Pipeline de imagem

Ordem alvo:

```
sombras → G-buffer → céu/IBL (gera IBL) → SSAO → iluminação → céu (fundo)
        → transparência → velocity/TAA → névoa → DOF → motion blur → bloom
        → exposição → ACES → color grading → gama → grade do editor → UI
        → imagem final
```

O céu é gerado antes da iluminação para alimentar o IBL; a composição do fundo acontece depois da iluminação, atrás da geometria opaca.

A R2 entrega um recorte menor:

```
sombra → G-buffer → iluminação → grade → TAA → bloom → exposição → ACES → gama
```

A grade é sobreposição do editor, não conteúdo do jogo. No alvo ela é desenhada depois do pós, antes da UI, e o modo jogo não a desenha; a R2 ainda a desenha dentro do quadro, como estado entregue.

O trabalho acontece em HDR linear (`Rgba16Float`) até o pós, que aplica a exposição, o mapeamento de tons e a conversão para a imagem final (`Rgba8Unorm`).

Contrato de entrada do quadro: itens de render, câmera (visão e projeção), ambiente e luzes. O renderizador não conhece entidades nem componentes; ele desenha o que recebe. O viewport monta essa lista a partir da cena.

## 5. Geometria, malha e material

A R2 desenha um cubo fixo para cada item, com posição e normal. O alvo substitui isso por malha e material de verdade.

Item de render:

| Campo | Conteúdo |
|---|---|
| `world` | matriz de mundo |
| `mesh` | identificador da malha cozida |
| `material` | identificador do material |
| `lod` | LOD escolhido |
| `skin` | identificador do esqueleto, quando houver |
| `flags` | sombra, visibilidade, alpha |

- A malha vem do cozido (`FORMATOS` §7.1): streams, índices, submalhas, LODs e bounds.
- O material vem do cozido (`FORMATOS` §7.6): PBR metallic-roughness ou cel.
- Submalha escolhe o material por slot; a sobrescrita do componente `caligo.model` vence o material padrão do modelo.
- O G-buffer é preenchido por submalha, não por malha inteira.
- Skinning roda no passe de vértice, com até quatro influências.
- Alpha opaco, máscara e blend têm filas separadas; transparente é desenhado depois da iluminação e ordenado por profundidade.
- Instancing é obrigatório para vegetação, props repetidos e multidão; o item pode agrupar várias instâncias da mesma malha e material.
- Culling por frustum e por oclusão entra na R5; a R3 faz frustum.
- LOD é escolhido por distância e cobertura de tela; HLOD entra na R5.

## 6. G-buffer

Alvo:

| Alvo | Formato | Conteúdo |
|---|---|---|
| RT0 | `Rgba8UnormSrgb` | albedo rgb; alpha ou cobertura |
| RT1 | `Rgba16Float` | normal octaédrica xy; rugosidade; metal |
| RT2 | `Rgba16Float` | emissivo rgb; oclusão |
| Profundidade | `Depth32Float` | profundidade de cena |
| Velocity | `Rg16Float` | movimento de tela por pixel |

A R2 usa três alvos: albedo com máscara, normal de mundo e profundidade. A migração para o G-buffer PBR é parte da R3.

O passe de iluminação reconstrói a posição de mundo a partir da profundidade e da matriz inversa de visão-projeção. A normal é decodificada da octaédrica. O material é lido de RT1 e RT2.

## 7. Iluminação

A R2 tem uma luz direcional. O alvo é iluminação diferida com várias luzes:

- Sol e lua do ambiente da cena, com direção, cor, intensidade e raio angular.
- Luzes pontuais e spot locais, com alcance, cor, intensidade, cookie e sombra opcional.
- Culling de luzes em clusters: tiles de 16 × 16 pixels e fatias de profundidade; cada cluster recebe uma lista de índices de luz.
- Limite de 512 luzes por quadro e 64 luzes por cluster; o excedente é descartado com aviso no editor, não silenciosamente.
- A iluminação avalia só as luzes do cluster do pixel.
- O ambiente contribui com IBL de céu e luz de chão; não há luz ambiente constante chapada.
- A exposição e o mapeamento de tons ficam no pós, não na luz.

A R3 entrega sol e lua; luzes pontuais e spot com culling simples entram na R5, junto do cluster completo e da cidade.

## 8. Sombras

A R2 entrega um mapa direcional com PCF 3 × 3 e deslocamento de profundidade.

Alvo:

- Cascatas: até quatro, com divisão estável e projeção ajustada ao texel.
- Resolução por cascata: 1024 ou 2048, conforme qualidade.
- PCSS: busca de bloqueador e penumbra variável; PCF como fallback de qualidade baixa.
- Sombras de luzes locais: pontuais e spot, com limite por orçamento.
- Estabilidade: a matriz da luz não treme com a câmera; o teste de 300 quadros cobre isso.
- Sombra de personagem e de objeto dinâmico entra com skinning, na R4.

A R3 entrega CSM com PCF melhorado; PCSS entra na R5. A R2 ainda usa um único mapa direcional; a R3 passa a cascatas.

## 9. Estabilização temporal

A R2 reprojeta pela profundidade e prende o histórico à vizinhança 3 × 3.

Alvo:

- Velocity buffer por objeto, incluindo skinning e movimento de câmera.
- Reprojeção por movimento de tela, não por profundidade.
- Rejeição de histórico por desoclusão: profundidade, normal e mudança de material.
- Jitter subpixel na projeção, sequência de Halton.
- Peso de histórico adaptativo por movimento e luminância.
- Teste de 300 quadros com câmera e objeto em movimento, sem tremor e sem arrasto fantasma.
- O histórico usa dois alvos em `Rgba16Float` que alternam a cada quadro.

A R3 introduz o velocity buffer; a R2 continua com reprojeção por profundidade até lá.

## 10. Estilização

O alvo estilizado é parte da identidade da Caligo, não um filtro por cima.

- Material com `shadingModel = cel`: rampa de luz em textura 1D, faixas configuráveis, especular em limiar e rim light.
- Contorno por material: cor, largura em pixels e máscara.
- Personagem e objeto skinnado usam contorno por casco invertido, para manter a espessura.
- Geometria estática pode usar contorno por profundidade e normal, mais barato.
- O contorno não cobre o céu, a grade do editor nem a UI.
- O cel shading convive com PBR no mesmo quadro; a escolha é por material.
- A R4 entrega o primeiro material cel e o contorno do personagem; a R5 amplia para o mundo.

## 11. Pós-processamento

Ordem alvo:

1. **SSAO/GTAO** — oclusão de ambiente sobre o G-buffer, antes da luz.
2. **Bloom** — extrai o brilho acima do limiar, borra e soma de volta.
3. **Névoa** — altura e distância, com cor do ambiente.
4. **DOF** — profundidade de campo em foco manual ou automático.
5. **Motion blur** — por velocity buffer, com clamp para não borrar a UI.
6. **Exposição** — escala o HDR antes do mapeamento de tons.
7. **ACES** — comprime a faixa dinâmica preservando a saturação.
8. **Color grading** — LUT ou ajuste de contraste, saturação e balanço.
9. **Gama** — converte para o espaço de exibição.
10. **UI** — compõe a UI de jogo depois do pós, em espaço de exibição.

A R2 entrega bloom, exposição, ACES e gama. A R3 acrescenta névoa; a R5 acrescenta SSAO, SSR em superfícies molhadas e grading; a R6 acrescenta DOF e motion blur.

SSR é reservado a superfícies molhadas, com máscara de rugosidade; não é reflexo global em tudo.

## 12. Céu, ambiente e IBL

O céu é procedural, não uma textura pintada.

- Zênite, horizonte e chão definem o gradiente.
- Sol e lua têm disco, halo e raio angular.
- A hora do dia move o sol e muda a cor; o clima muda nuvens, névoa e umidade.
- O céu é renderizado em um cubemap de baixa resolução e filtrado para IBL: irradiância difusa e especular pré-filtrado.
- O ambiente da cena fornece luz difusa do céu e luz de chão; o material PBR usa isso como ambiente.
- A exposição do céu acompanha a exposição da cena.
- O céu não é desenhado no G-buffer; ele é um passe de fundo e uma fonte de IBL.
- A R3 entrega céu procedural com ciclo e IBL básico; nuvens e clima completo entram na R5.

## 13. Streaming, LOD e culling

- Malha e textura têm orçamento de memória e carregamento assíncrono.
- Textura carrega mips em ordem; o nível alto primeiro.
- Malha carrega LODs sob demanda; o LOD0 pode ficar residente e os demais streamados.
- Terreno carrega por tile; HLOD cobre a distância.
- Vegetação e props usam instancing e LOD por distância.
- Culling por frustum na R3; oclusão e culling de luzes na R5.
- Nenhum carregamento pode gerar hitch acima do orçamento de 16 ms.
- Descarregar recurso não pode invalidar item em uso no quadro.

## 14. Orçamento

- Alvo: 1080p60 em GTX 1060 6 GB (D-018).
- Piso: FL11_0, 2 GB de VRAM, 8 GB de RAM (D-019).
- Frame de 16,6 ms de CPU e 16,6 ms de GPU.
- Regressão acima de 10% no orçamento quebra a release.
- O renderer expõe contadores por passe: draw calls, triângulos, luzes, clusters, memória de textura e tempo de GPU quando disponível.
- Qualidade escala por preset: Low, Medium, High e Ultra; o preset muda cascatas, resolução de sombra, SSAO, SSR e streaming, não a correção da imagem.

## 15. Testes

- Desenho fora da tela em WARP, exercitando a camada de recurso gráfico de ponta a ponta.
- Imagem dourada por sistema: sombra, PBR, céu/IBL, cel shading, contorno, SSAO, névoa, DOF, motion blur e grading.
- Estabilidade do TAA por 300 quadros, com câmera e objeto em movimento, sem tremor e sem arrasto.
- Determinismo: mesma cena e mesma câmera produzem a mesma imagem.
- Grafo: ordem dos passes, resolução de recurso, reaproveitamento de memória e recusa de leitura antes da escrita.
- Comparação de imagens: igualdade, tolerância, contagem acima da tolerância e tamanhos incompatíveis.
- Culling de luzes: cluster sem luz, cluster com uma luz e cluster no limite.
- Orçamento: contadores de draw call, triângulo e memória; regressão acima de 10% falha.

## 16. O que entra em cada release

| Release | Render |
|---|---|
| R2 | RHI, grafo, G-buffer simples, luz direcional, PCF 3 × 3, TAA por profundidade, bloom, exposição, ACES e gama |
| R3 | malha e material, PBR metallic-roughness, textura, céu procedural com ciclo, IBL básico, CSM, TAA com velocity e névoa |
| R4 | skinning, animação no vertex shader, cel shading e contorno do personagem |
| R5 | luzes pontuais e spot com clusters, instancing, HLOD, SSAO, SSR em superfícies molhadas e clima |
| R6 | DOF, motion blur, color grading e composição da UI de jogo |
| R7 | polimento, orçamento e qualidade de produção |

A ordem é a inicial; ela pode mudar por release, mas o princípio é fixo: o renderer evolui em estágios, não em reescrita monolítica (D-045).

## 17. Fora de escopo na v1

- Ray tracing obrigatório e fotorrealismo (D-015).
- DX12 na v1 (D-004).
- Shader graph e material por grafo (D-013).
- Renderer separado para o editor; o mesmo renderizador desenha os dois.
- Plataforma que não Windows x64 (D-004, D-012).

## 18. Histórico

- **0.2** — separa R2 entregue do alvo; define malha e material, G-buffer PBR, CSM/PCSS, clusters de luz, velocity TAA, cel shading com contorno, pós completo, céu/IBL, streaming e os estágios por release.
- **0.1** — pipeline diferido da R2, sombra direcional, TAA por profundidade e pós.
