# Caligo — Scripting e Play

Versão 0.1

## 1. Responsabilidade

O scripting de gameplay é C# sobre .NET 10 LTS. O editor e o host nativo continuam em C++. A fronteira é uma ABI C explícita, sem STL, exceção ou ponteiro sem contrato. Sem scripts, a cena continua abrindo e o editor continua funcionando.

O jogo declara seus tipos em `Caligo.Gameplay.GameModule`, que implementa `Caligo.Scripting.IGameModule` e registra cada comportamento no `ScriptRegistry`. O editor lista esses tipos no inspetor e a cena guarda apenas o nome estável do tipo.

## 2. Modelo de script

```csharp
public sealed class Spinner : ScriptBehaviour
{
  public override void OnCreate()
  {
    _baseRotation = Transform.Rotation;
  }

  public override void OnUpdate(float deltaSeconds)
  {
    Transform transform = Transform;
    transform.Rotation = Quaternion.Normalize(
        _baseRotation * Quaternion.CreateFromAxisAngle(Vector3.UnitY, _angle));
    Transform = transform;
  }
}
```

- `OnCreate` roda quando o script é anexado no início da execução.
- `OnUpdate` roda no tick fixo de 60 Hz.
- `OnDestroy` roda ao parar a execução, inclusive quando há erro.
- `Transform` é local à entidade, não o mundo.
- Uma entidade pode ter vários scripts, mas nunca o mesmo tipo repetido.
- Nomes de script começam com letra e usam apenas letras, dígitos e sublinhado; o nome é o contrato da cena.

Entrada exposta nesta fatia: `W`, `A`, `S`, `D`, `Space`, `LeftShift`, setas e `Escape`, consultadas por `IsKeyDown` e `IsKeyPressed`. A entrada chega ao script como um snapshot do quadro, sem expor `platform` ao assembly gerenciado.

## 3. Ciclo de execução

O play-in-editor trabalha sobre a cena aberta:

1. A cena inteira é copiada para um snapshot.
2. O host gerenciado reinicia, recarrega o módulo do jogo e instancia os scripts das entidades.
3. A simulação avança em tick fixo de 60 Hz, usando a câmera do editor.
4. Pausa congela o tempo; o avanço de quadro executa exatamente um tick.
5. Parar descarta o estado simulado e restaura o snapshot, sem contaminar a autoria.

Durante a execução a edição fica bloqueada: hierarquia, transformação, nomes, scripts e gizmo não alteram a cena. Salvar, abrir e criar cena também ficam indisponíveis até parar. A execução nunca grava no arquivo de cena.

Atalhos: `F5` inicia ou para, `F6` pausa ou continua, `F7` avança um quadro quando pausado e `F8` recarrega os scripts. Recarregar durante a execução reinicia o host e reanexa os scripts à cena atual.

## 4. Hospedagem nativa

- `hostfxr` é carregado de um runtime .NET privado em `<exe>/runtime/dotnet`.
- O bootstrap `Caligo.Scripting.Loader` não é coletável e expõe funções `[UnmanagedCallersOnly]`.
- O bootstrap cria um `AssemblyLoadContext` coletável, copia os assemblies para uma pasta temporária e carrega `Caligo.Scripting.dll` e `Caligo.Gameplay.dll` nesse contexto.
- Cada início de execução descarrega o contexto anterior; o shadow copy evita bloquear os arquivos originais.
- O host nativo fica carregado no editor desde o início, para listar tipos no inspetor; só o contexto gerenciado é recriado.

Layout no artefato:

```
caligo.exe
runtime/
  dotnet/            runtime .NET privado
  host/              Caligo.Scripting.Loader.dll e runtimeconfig
  gameplay/          Caligo.Scripting.dll e Caligo.Gameplay.dll
```

## 5. Erros

Falha ao carregar o runtime, registrar um tipo, anexar um script ou executar um tick vira diagnóstico no console do editor e interrompe a execução quando necessário. Um script que falha não deixa a simulação rodando em estado parcial: a cena volta ao snapshot.

## 6. Testes

- Testes gerenciados cobrem registro de tipos, nomes inválidos, anexação duplicada, delta inválido, rotação, movimento e normalização diagonal.
- O teste nativo cobre o host sem runtime disponível.
- O smoke de linha de comando carrega o runtime privado, inicia a execução, roda 60 ticks, confere o movimento e a rotação e encerra o host.
- O CI roda os testes gerenciados antes do build e o smoke dentro do `ctest`, em Debug e Release.