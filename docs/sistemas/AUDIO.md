# Caligo — Áudio

Versão 0.1

## 1. Responsabilidade

O sistema de áudio reproduz som no espaço da cena. Ele se divide em dois: um dispositivo que entrega amostras à placa e um misturador que compõe as vozes em um buffer intercalado. O misturador é a parte que a engine controla; o dispositivo é a parte que toca o som de verdade.

O misturador não depende do dispositivo. Ele é uma unidade pura — recebe vozes, mistura, devolve amostras — e por isso roda em teste sem hardware algum. O dispositivo aceita um backend nulo, o que permite subir o áudio inteiro no CI, onde não existe placa de som.

O módulo `audio` depende de `core`; quando o módulo de assets existir, ele também decodifica conteúdo cozido.

## 2. Clipe

Um clipe é uma amostra de som em memória:

- PCM em ponto flutuante, intercalado por canal.
- Criado de amostras em memória ou de um arquivo de áudio.
- Expõe contagem de quadros, canais, taxa de amostragem, duração e a amostra de um quadro e canal.

A leitura de arquivo decodifica para ponto flutuante. A contagem de quadros lida em cada passo vem pelo parâmetro de saída do decodificador, e não pelo código de retorno: um fim normal não é uma falha, e confundir os dois faz a decodificação parar no primeiro bloco.

## 3. Misturador

O misturador compõe as vozes ativas em um buffer de saída com a taxa e a contagem de canais pedidas na criação. Cada voz vem de uma descrição:

| Campo | Papel |
|---|---|
| clipe | a amostra de som |
| posição | o ponto no mundo |
| volume | o ganho da voz |
| tom | o fator de velocidade |
| distância mínima | abaixo dela não há atenuação |
| distância máxima | acima dela a voz silencia |
| espacial | liga a atenuação por posição |
| repetir | reinicia ao terminar |

Espacialização:

- A distância entre voz e ouvinte define um ganho: até a distância mínima o som toca inteiro; daí até a máxima o ganho cai; além da máxima o som silencia.
- A direção da voz projetada no eixo direito do ouvinte define o equilíbrio entre os canais, de um lado ao outro, passando pelo centro.
- O ganho de distância é aplicado aos dois canais, junto do equilíbrio. Sem espacialização, a voz toca igual nos dois canais.

Cada voz ativa tem um identificador de 64 bits que combina uma geração e o índice no vetor. A geração sobe quando o índice é reutilizado, então um identificador antigo nunca se confunde com uma voz nova no mesmo lugar. Parar uma voz, parar todas, perguntar se uma voz toca, contar as vozes ativas e contar os quadros já misturados completam o contrato.

O ouvinte tem posição, frente e cima. Ele é lido e escrito sob trava; a leitura devolve uma cópia.

O misturador esconde a implementação atrás de um ponteiro opaco; o cabeçalho declara o contrato e cópia e movimento são negados.

## 4. Dispositivo

O dispositivo abre a reprodução, inicia, para e responde se está rodando. A descrição escolhe taxa, canais e o modo sem saída de som:

- Sem o modo sem saída, o dispositivo abre o backend padrão do sistema.
- Com o modo sem saída, ele abre um backend nulo: não há placa, mas o resto do caminho roda igual, e o CI consegue exercitar o dispositivo.

O dispositivo tem um misturador próprio. Quando o backend pede amostras, o dispositivo mistura o quadro e entrega o buffer. Um erro ao abrir o dispositivo não deixa contexto aberto para trás.

A saída aceita um ou dois canais; mais que isso é recusado na criação.

## 5. Erros

Abrir um arquivo de áudio, criar um clipe, iniciar o dispositivo: cada passo devolve um resultado com valor ou com erro. Um arquivo sem amostras válidas não vira clipe vazio — vira erro. Um dispositivo sem contexto ou sem reprodução falha na criação, e não em uso.

## 6. Testes

- O clipe: leitura de amostras de um arquivo, canais, taxa, duração, erro para arquivo ausente e rejeição de canal além do limite.
- O misturador: voz sem espacialização nos dois canais, fim do clipe, repetição, atenuação por distância com silêncio no limite, equilíbrio entre canais, tom pela taxa do clipe, parada e contagem de vozes e quadros.
- O dispositivo: criação em modo sem saída, início e parada, e mistura pela chamada de áudio.

O dispositivo de teste usa sempre o backend nulo; nenhum teste depende de hardware de som.
