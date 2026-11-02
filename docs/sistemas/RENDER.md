# Caligo — Renderização

Versão 0.1

## 1. Responsabilidade

O sistema de renderização transforma a cena em imagem. Ele cobre a camada de recurso gráfico, o grafo que ordena os passes e o pipeline diferido com luz, sombra, estabilização temporal e pós-processamento. O editor e o runtime usam o mesmo renderizador; não há um caminho de desenho para a ferramenta e outro para o jogo.

A v1 desenha sobre Direct3D 11 em FL11_0, alvo fixo da release. A camada de recurso gráfico nasce pequena e explícita, preparada para DX12 sem prometer DX12. Fora do alvo: ray tracing e fotorrealismo. A prioridade é estabilidade e custo previsível.

O renderizador é determinístico sob WARP, o que permite validar a imagem no CI sem GPU dedicada.

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

Formatos desta release:

| Formato | Uso |
|---|---|
| `Rgba8Unorm` | imagem final apresentada |
| `Rgba16Float` | HDR linear, G-buffer e histórico temporal |
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

A ordem dos passes em cada quadro:

```
sombra → G-buffer → iluminação → grade → TAA → pós → imagem final
```

O trabalho acontece em HDR linear (`Rgba16Float`) até o pós, que aplica a exposição, o mapeamento de tons e a conversão para a imagem final (`Rgba8Unorm`).

Contrato de entrada do quadro: itens de render (transform de mundo e cor), câmera (visão e projeção) e luz (direção e cor). O renderizador não conhece entidades nem componentes; ele desenha o que recebe. O viewport monta essa lista a partir da cena.

## 5. Sombra direcional

Uma luz direcional projeta um mapa de profundidade a partir do ponto de vista da luz. Detalhes:

- PCF 3×3 sobre o mapa, com deslocamento de profundidade contra o serrilhado.
- O tamanho de texel e a matriz da luz são levados ao passe de iluminação.
- Os recursos do mapa são criados mesmo quando não há item a desenhar, para que o passe de iluminação sempre tenha uma amostra válida.
- A geometria de malha é compartilhada entre o mapa de sombra e o G-buffer.

## 6. Iluminação diferida

O G-buffer guarda três alvos:

| Alvo | Conteúdo |
|---|---|
| Albedo (`Rgba16Float`) | cor nos canais rgb; máscara no canal alfa |
| Normal (`Rgba16Float`) | normal de mundo |
| Profundidade (`Depth32Float`) | profundidade de cena |

A iluminação é um passe de tela cheia: reconstrói a posição de mundo a partir da profundidade e da matriz inversa de visão-projeção, consulta o mapa de sombra e escreve o resultado em HDR. O fundo entra pela cor de fundo da cena.

A grade do chão é desenhada depois da luz, sobre o HDR, usando a profundidade do G-buffer para teste e sem escrever profundidade. A grade é desenho do editor, não geometria da cena.

## 7. Estabilização temporal

A estabilização temporal remove o tremor da imagem em movimento:

- A projeção recebe um deslocamento subpixel por quadro na sequência de Halton (bases 2 e 3).
- Cada pixel é reprojetado pela profundidade no histórico.
- A cor do histórico é presa à vizinhança 3×3 do quadro atual, para reduzir arrasto em contornos.
- A mistura entre atual e histórico usa peso fixo baixo, favorecendo o histórico acumulado.
- O histórico usa dois alvos em `Rgba16Float` que alternam a cada quadro.

## 8. Pós-processamento

Sobre o resultado temporal, o pós-processamento aplica, nesta ordem:

1. **Bloom** — extrai o brilho acima do limiar, borra e soma de volta.
2. **Exposição** — escala o HDR antes do mapeamento de tons.
3. **Mapeamento de tons ACES** — comprime a faixa dinâmica preservando a saturação.
4. **Gama** — converte para o espaço de exibição.

A saída é `Rgba8Unorm`, pronta para apresentação ou para leitura de pixels.

## 9. Leitura da imagem

O renderizador expõe a imagem final como visão de textura para o editor desenhar no painel e como leitura de pixels para o teste. A leitura devolve os bytes da imagem final e é o ponto de partida da validação de imagem.

A comparação de imagens é uma função própria: recebe esperado e obtido, mais uma tolerância, e devolve a maior diferença por canal, a contagem de pixels divergentes e o total. Ela não depende do dispositivo, o que permite testá-la isoladamente.

## 10. Testes

- Desenho fora da tela em WARP, exercitando a camada de recurso gráfico de ponta a ponta.
- Cubos sólidos e iluminados; profundidade correta e determinismo.
- Sombra direcional projetada no chão.
- Pós-processamento espalhando o brilho de objetos intensos.
- Estabilidade do TAA por 300 quadros e determinismo com a câmera em movimento.
- Grafo: ordem dos passes, resolução de recurso, reaproveitamento de memória e recusa de leitura antes da escrita.
- Comparação de imagens: igualdade, tolerância, contagem acima da tolerância e tamanhos incompatíveis.
