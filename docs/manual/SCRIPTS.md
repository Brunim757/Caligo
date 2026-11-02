# Caligo — Manual de scripts

Versão 0.1 — R1

## 1. Como um script entra na cena

O jogo registra seus comportamentos no módulo de jogo. O editor lê essa lista e a oferece no inspetor. A cena guarda apenas o **nome** do tipo.

O nome é o contrato: começa com letra, usa apenas letras, dígitos e sublinhado, e vale até 128 caracteres. Renomear a classe quebra as cenas que a usam.

## 2. Escrever um comportamento

Um comportamento é uma classe que herda de `ScriptBehaviour` e sobrescreve os retornos de chamada que precisar.

```csharp
using System.Numerics;
using Caligo.Scripting;

public sealed class Gira : ScriptBehaviour
{
  private Quaternion _inicial;
  private float _angulo;

  public override void OnCreate()
  {
    _inicial = Transform.Rotation;
  }

  public override void OnUpdate(float deltaSeconds)
  {
    _angulo += deltaSeconds;
    Transform transform = Transform;
    transform.Rotation = Quaternion.Normalize(
        _inicial * Quaternion.CreateFromAxisAngle(Vector3.UnitY, _angulo));
    Transform = transform;
  }

  public override void OnDestroy()
  {
  }
}
```

O que cada retorno de chamada faz:

- **OnCreate** — quando o script é anexado ao início da execução.
- **OnUpdate** — a cada tick, com o tempo decorrido em segundos. Tick fixo de 60 Hz.
- **OnDestroy** — ao parar a execução, inclusive quando ela termina por erro.

A propriedade **Transform** é local à entidade, relativa ao pai. Não é o transform de mundo.

Também estão disponíveis:

- **EntityId** — o identificador da entidade.
- **IsKeyDown** — a tecla está pressionada.
- **IsKeyPressed** — a tecla foi pressionada neste quadro.
- **Log** — escreve no console do editor, com o nome da classe na frente.

Teclas expostas nesta release: `W`, `A`, `S`, `D`, `Space`, `LeftShift`, setas e `Escape`.

## 3. Registrar

O módulo de jogo registra cada comportamento. Só o que está registrado aparece no inspetor.

```csharp
using Caligo.Scripting;

namespace Caligo.Gameplay;

public sealed class GameModule : IGameModule
{
  public void Configure(ScriptRegistry registry)
  {
    registry.Register<Gira>();
  }
}
```

Registrar o mesmo nome duas vezes é erro, e o editor mostra isso no console.

## 4. Compilar

A engine **não compila C# durante a execução**. Os projetos são compilados na construção do executável, e o pacote já vem com os assemblies prontos: quem usa o pacote não tem o que instalar nem compilar.

Ao construir a partir do código, a compilação gera os assemblies e os coloca na pasta `runtime/gameplay` ao lado do executável.

Depois de compilar, **F8** recarrega os scripts sem fechar o editor. A recarga descarrega a versão anterior, copia os assemblies para uma pasta temporária e carrega a nova.

Se você acrescentou um tipo, ele aparece em **Adicionar script** depois da recarga. Se o editor estava executando, a execução reinicia com a cena atual.

## 5. Executar

- **F5** inicia ou para.
- **F6** pausa ou continua.
- **F7** avança exatamente um quadro, quando pausado.
- **F8** recarrega os scripts.

O que acontece ao iniciar:

1. A cena é copiada para um instantâneo.
2. Os scripts do jogo são recarregados e cada script da cena é criado.
3. A simulação avança em tick fixo de 60 Hz, com a câmera do editor.

Durante a execução a cena é somente leitura: hierarquia, inspetor, gizmo, criar, excluir, salvar, abrir e criar cena ficam bloqueados. Nada da execução é gravado no arquivo.

Ao parar, o instantâneo volta. O que aconteceu durante o play não fica na cena, e o que estava salvo continua salvo.

## 6. Quando algo dá errado

Um tipo não registrado, um nome inválido, um script repetido ou uma falha em um tick aparece no console do editor.

Uma falha durante o tick **interrompe a execução** e devolve a cena ao instantâneo: não existe simulação em estado parcial. O erro fica registrado com o nome do script.

Uma cena sem scripts abre e funciona como qualquer outra. Script é uma camada, não um requisito para abrir o editor.