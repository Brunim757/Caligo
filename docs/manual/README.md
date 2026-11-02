# Caligo — Manual

Versão 0.2 — R2

O manual descreve o que a engine faz na release atual. Cada release acrescenta a sua parte; nada aqui é escrito para uma capacidade que ainda não existe.

## Onde começar

- [Editor](EDITOR.md) — abrir, enxergar a cena, criar e transformar entidades, salvar.
- [Scripts](SCRIPTS.md) — escrever um comportamento, compilar e executar dentro do editor.

## O que esta versão entrega

Da R1:

- Editor com hierarquia, inspetor, viewport com gizmo e console.
- Cena com entidades, hierarquia e transformação, em arquivo `.cscene`.
- Componentes de script: vários por entidade, escolhidos no inspetor.
- Execução dentro do editor, com pausa, avanço de quadro e recarga de scripts.
- Gravação e abertura de cena, com proteção contra perda de alterações.

Da R2:

- Viewport com cena sombreada: as caixas ganharam sólido, luz e profundidade.
- Uma luz direcional, com sombra projetada no chão.
- Imagem estável em movimento, sem tremor.
- Correção de cor e brilho: exposição, mapeamento de tons e realce de luz intensa.
- Base de áudio: um misturador espacial, ainda sem superfície de autoria.

## O que ainda não existe

Isto não é uma lista de defeitos: é o recorte da release.

- Luz única: só existe uma luz direcional. Luzes pontuais, ambiente e céu entram depois.
- Material: toda superfície usa a mesma cor neutra. Um sistema de material entra depois.
- Física: não há colisão, gravidade nem personagem.
- Câmera livre no jogo: durante a execução usa-se a câmera do editor.
- Projetos: a engine trabalha com um conteúdo único ao lado do executável.
- Cook e assets importados.
- Áudio sem autoria: o misturador existe por baixo, mas ainda não há como escolher sons no editor.

## Requisitos

Windows 10 ou 11, 64 bits. O pacote já traz o runtime necessário; não é preciso instalar nada.