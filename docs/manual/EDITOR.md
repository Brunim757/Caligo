# Caligo — Manual do editor

Versão 0.1 — R1

## 1. Abrir

Execute `caligo.exe`. O editor abre com a cena de exemplo que acompanha o pacote.

Para abrir outra cena direto: `caligo.exe --scene <caminho>`.

Sem argumentos, o editor procura a cena padrão ao lado do executável. Se ela não existir, a cena começa vazia.

## 2. A tela

O layout nasce em quatro áreas:

- **Cena** — o viewport, no centro.
- **Hierarquia** — a árvore de entidades, à esquerda.
- **Inspetor** — os dados da entidade selecionada, à direita.
- **Console** — as mensagens do editor, embaixo.

As áreas podem ser arrastadas e reordenadas. O menu **Exibir → Restaurar layout** devolve a organização original.

## 3. O que aparece no viewport

Nesta release a cena é desenhada como **caixas de arame**. Cada caixa é o transform de uma entidade: posição, rotação e escala. O chão é uma caixa larga e baixa, um prédio é uma caixa alta, uma janela é uma caixa fina.

Não há malha, material nem luz ainda. A caixa não é um substituto de modelo: ela é a forma do transform, e continua existindo depois que os modelos entrarem, como guia de seleção.

A grade do chão, os marcadores das entidades e a linha até o chão são desenho do editor, não geometria da cena.

## 4. Navegar

- **Botão direito arrastado** — orbita ao redor do alvo.
- **Botão do meio arrastado** — desloca o alvo.
- **Roda do mouse** — aproxima e afasta.
- **F** — enquadra a entidade selecionada.

A navegação só começa quando o mouse está sobre o viewport. Um arraste iniciado continua mesmo se o mouse sair da área.

## 5. Transformar

Três ferramentas, no topo do viewport:

- **Mover (W)**
- **Girar (E)**
- **Escalar (R)**

O botão **Espaço local / Espaço global** alterna entre os eixos da entidade e os eixos do mundo.

Arraste um eixo do gizmo para transformar. Segure **Ctrl** durante o arraste para encaixar: meia unidade ao mover, quinze graus ao girar, um décimo ao escalar.

O mesmo se faz pelo inspetor, com os campos **Posição**, **Rotação** (em graus) e **Escala**. **Redefinir transformação** devolve a entidade à origem.

A escala não aceita zero.

## 6. Criar e organizar

- **+ Entidade** na hierarquia, ou **Ctrl+Shift+N**, cria uma entidade como filha da que está selecionada. Sem seleção, cria na raiz.
- Arrastar uma entidade sobre outra na hierarquia muda o pai.
- O menu de contexto da hierarquia tem **Criar filha**, **Remover pai** e **Excluir**.
- **Delete** exclui a entidade selecionada, com as filhas.
- O nome é editado no campo do inspetor.

A hierarquia é uma árvore: o transform do filho é relativo ao pai. Mover o pai move os filhos.

## 7. Componentes

Toda entidade tem **transform**, obrigatório. Não existe entidade sem ele.

**Scripts** é opcional e a seção mostra a contagem: `Scripts (0)` numa entidade nova, `Scripts (1)` com um script anexado.

- **Adicionar script** lista os tipos que o jogo registrou.
- **Remover** desanexa um script.
- Uma entidade pode ter vários scripts diferentes, mas não o mesmo tipo duas vezes.

Entidade nova nasce só com transform. Nada é anexado automaticamente.

## 8. Arquivo

- **Ctrl+N** — nova cena.
- **Ctrl+O** — abrir cena.
- **Ctrl+S** — salvar.
- **Ctrl+Shift+S** — salvar como.
- **Ctrl+Q** — sair.

Quando há alterações não salvas, o editor pergunta antes de trocar de cena ou sair, com três saídas: salvar e continuar, descartar, cancelar.

O título da janela mostra o nome do arquivo e sinaliza quando há alteração pendente.

## 9. Console

Mostra as mensagens do editor: cena aberta, cena salva, entidade criada, script anexado, início e fim de execução, e os erros.

- O seletor de nível esconde mensagens abaixo do nível escolhido.
- **Limpar** esvazia a lista.
- **Rolagem automática** acompanha as mensagens novas.
- O menu de contexto de uma linha copia o texto.

## 10. Atalhos

| Atalho | Ação |
|---|---|
| `Ctrl+N` | Nova cena |
| `Ctrl+O` | Abrir cena |
| `Ctrl+S` | Salvar |
| `Ctrl+Shift+S` | Salvar como |
| `Ctrl+Q` | Sair |
| `Ctrl+Shift+N` | Criar entidade |
| `Delete` | Excluir entidade |
| `F` | Enquadrar seleção |
| `W` | Mover |
| `E` | Girar |
| `R` | Escalar |
| `Ctrl` (durante o arraste) | Encaixar |
| `F5` | Iniciar ou parar a execução |
| `F6` | Pausar ou continuar |
| `F7` | Avançar um quadro |
| `F8` | Recarregar scripts |

`F5` a `F8` funcionam em qualquer lugar do editor. W, E, R e F só com o mouse sobre o viewport.

## 11. Problemas comuns

**"Os scripts do jogo não estão disponíveis."** O editor não encontrou o runtime ao lado do executável. Verifique se a pasta `runtime` veio junto do pacote e use **F8** para tentar carregar de novo.

**A cena não abre.** O console mostra o motivo: arquivo ausente, texto inválido, componente desconhecido, entidade sem transform ou script repetido.

**O play não inicia.** A execução depende de os scripts carregarem. Se falharem, a mensagem aparece no console e a cena continua intacta.