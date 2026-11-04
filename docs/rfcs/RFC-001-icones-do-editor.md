# RFC-001 — Ícones do editor em camadas

- **Status:** aceito
- **Data:** 2026-11-04
- **Altera:** D-032 (FIXO)
- **Documentos afetados:** `docs/DECISOES.md`, `docs/ESCOPO.md`, `docs/sistemas/EDITOR.md`, `docs/sistemas/ASSETS.md`

## 1. Problema

A D-032 fixa "ícones Lucide embutidos como atlas". O Lucide é um conjunto **só de contorno**, com traço uniforme, pensado para botões. Ele é adequado para a toolbar e o gizmo, mas fraco como ícone de linha do asset browser: a 16 px um contorno fino não distingue tipo de arquivo e não entrega o visual sólido que uma ferramenta de produção — e o próprio Unity e o Unreal — usam no navegador.

Além disso, o Lucide não tem glifo para tipo algum da engine (`.cproject`, `.cmeta`, `.cscene`, `.cterrain`, `.casset`, `.cui`) nem para linguagem (o `.cs` caía num `file-code` genérico). A primeira versão da D-032 resolvia isso por aproximação de glifo, o que é aceitável para a UI, não para o tipo do asset.

O editor precisa de dois trabalhos visuais distintos:

- **Ícone de ação** (toolbar, botão, gizmo) — traço, monocromático, denso.
- **Ícone de tipo** (asset browser, inspetor, abas, hierarquia) — sólido, legível a 16 px, com cor por tipo.

Um único conjunto não cobre os dois bem.

## 2. Opções consideradas

| Conjunto | Versão | Ícones | Estilo | Licença | Veredito |
|---|---|---|---|---|---|
| Lucide | 1.68.0 | 2 313 | só contorno | ISC | fica na UI |
| Phosphor | 2.1.1 | 1 512 × 6 pesos | thin a fill e duotone | MIT | descartado |
| Remix Icon | 4.9.1 | 3 229 | line + fill | Apache-2.0 | descartado |
| Material Symbols | 0.48.3 | ~3 600 | outline + filled | Apache-2.0 | descartado |
| Fluent UI System Icons | 1.1.345 | 4 524 nomes-base, 21 777 SVG | regular, filled e color; 16/20/24/28/32/48 | MIT | adotado para assets |
| Conjunto Caligo próprio | — | 7 códigos | preenchido | própria | adotado para tipos da engine |

Verificação feita nas listas reais dos pacotes: o Fluent contém `code_cs`, a família `document_*`, `folder_open`, `folder_zip`, a família `weather_*` completa, `cube_multiple`, `box_multiple`, `tree_evergreen`, `plant_grass`, `leaf_one`, `mountain_trail`, `globe_surface`, `paint_brush`, `paint_bucket`, `color_fill`, `layer`, `shapes`, `lock_closed`, `ruler`, `compass_northwest`, `navigation`, `gauge`, `bot`, `games`, `apps`, `window`, `data_usage` e `braces`. As variantes `_color` cobrem os tipos mais vistos.

O Fluent **não** tem glifo literal para `mountain`, `tree`, `sphere`, `cpu`, `magnet`, `layout` e `terrain`. Isso não é bloqueio: `mountain_trail`, `tree_evergreen` e `plant_grass` cobrem o conceito de perto, e os tipos verdadeiramente da engine ficam no conjunto próprio.

Os ícones do Unity e do Unreal são proprietários e não podem ser copiados. O logo C# é marca da Microsoft; o glifo `code_cs` do Fluent, sob MIT, resolve a linguagem sem usar marca fora de licença.

## 3. Custo

- Duas famílias (Lucide e Fluent) em vez de uma: dois avisos de licença e dois geradores de atlas.
- Sete SVGs próprios desenhados no mesmo grid de 24 px, no repositório, versionados como qualquer outro asset de UI.
- O manifesto `código → glifo` cresce; a UI continua lendo só o código.
- Nenhuma mudança de stack gráfica, de ImGui ou de ImGuizmo. A D-016 continua intacta.

## 4. Impacto

- **Contrato preservado.** O código semântico do ícone (`folder`, `scene`, `script`, `terrain`...) continua sendo o contrato. O glifo é implementação substituível: trocar o desenho não toca em dado, cena ou código.
- **Duas camadas + a própria.** Lucide na UI; Fluent nos assets; conjunto Caligo nos tipos da engine.
- **Cor por tipo.** Ícone de tipo pode usar a variante `_color`; a cor de estado (erro, aviso, sucesso) continua sobreposta.
- **Miniatura não é ícone.** Miniatura de modelo, textura e material continua na R7; o ícone do tipo entra na R3.
- **Cobre todo arquivo.** Nenhuma linha do navegador fica sem ícone, inclusive `.cproject` e arquivos da engine.

## 5. Decisão

A D-032 passa a valer com três camadas:

1. **Lucide 1.68.0** (ISC, contorno, grade 24 px) na UI: toolbar, botões, gizmo, menus.
2. **Fluent UI System Icons 1.1.345** (MIT, regular/filled/color) no restante: asset browser, inspetor, abas, hierarquia e arrasto.
3. **Conjunto Caligo próprio** (preenchido, grade 24 px, sete códigos) só para os tipos da engine: `project` (`.cproject`), `meta` (`.cmeta`), `scene` (`.cscene`), `terrain` (`.cterrain`), `cooked` (`.casset`), `ui` (`.cui`) e `ui-style` (`.cuistyle`).

O código semântico do ícone é estável e versionado em manifesto; o glifo de cada camada é a implementação. Trocar um glifo é trocar o manifesto, nunca dado ou código.

## 6. Consequências

- `docs/DECISOES.md`: D-032 reescrita; versão 1.0 → 1.1.
- `docs/ESCOPO.md`: linha da D-032, linha da stack e capacidade do editor; versão 1.3 → 1.4.
- `docs/sistemas/EDITOR.md`: §5.4 passa a descrever as três camadas; versão 0.1 → 0.2.
- `docs/sistemas/ASSETS.md`: §11.1 passa a mapear `código → glifo Fluent` mais os códigos próprios; versão 0.1 → 0.2.

## 7. Histórico

- **1.0** — 2026-11-04 — aceito. Substitui "Lucide para tudo" por Lucide na UI, Fluent nos assets e conjunto Caligo próprio para os tipos da engine.
