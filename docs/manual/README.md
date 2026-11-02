# Caligo — Manual

Versão 0.1 — R1

O manual descreve o que a engine faz na release atual. Cada release acrescenta a sua parte; nada aqui é escrito para uma capacidade que ainda não existe.

## Onde começar

- [Editor](EDITOR.md) — abrir, enxergar a cena, criar e transformar entidades, salvar.
- [Scripts](SCRIPTS.md) — escrever um comportamento, compilar e executar dentro do editor.

## O que esta versão entrega

- Editor com hierarquia, inspetor, viewport com gizmo e console.
- Cena com entidades, hierarquia e transformação, em arquivo `.cscene`.
- Componentes de script: vários por entidade, escolhidos no inspetor.
- Execução dentro do editor, com pausa, avanço de quadro e recarga de scripts.
- Gravação e abertura de cena, com proteção contra perda de alterações.

## O que ainda não existe

Isto não é uma lista de defeitos: é o recorte da release.

- Imagem sólida: a cena é desenhada como caixas de arame. Malha, material, luz, sombra e pós-processamento entram na R2.
- Física: não há colisão, gravidade nem personagem.
- Câmera livre no jogo: durante a execução usa-se a câmera do editor.
- Projetos: a R1 trabalha com um conteúdo único ao lado do executável.
- Cook e assets importados.

## Requisitos

Windows 10 ou 11, 64 bits. O pacote já traz o runtime necessário; não é preciso instalar nada.