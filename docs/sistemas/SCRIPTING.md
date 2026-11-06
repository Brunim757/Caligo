# Caligo — Scripting, Compilação e Play

Versão 0.2

## 1. Responsabilidade

O scripting de gameplay é C# sobre .NET 10 LTS. O editor e o host nativo continuam em C++. A fronteira é uma ABI C explícita, sem STL, exceção ou ponteiro sem contrato. Sem scripts, a cena continua abrindo e o editor continua funcionando.

Cada projeto tem o próprio assembly de gameplay. A engine compila esse assembly com o Roslyn embutido, pelo editor ou pela CLI, sem SDK, MSBuild, NuGet ou CI. O jogo produzido leva o assembly já compilado e o runtime .NET privado; a máquina do jogador não instala nada (D-041, D-042).

O desenvolvimento da engine usa o SDK .NET no CI para compilar os assemblies da própria engine e os testes. Isso não é requisito para quem produz um jogo.

## 2. Projeto e assembly

O descritor `.cproject` declara `gameplayProject` e `gameplayAssembly` (FORMATOS §3.1). A árvore padrão:

```
MeuJogo/
  MeuJogo.cproject
  scripts/
    MeuJogo.Gameplay/
      MeuJogo.Gameplay.csproj
      GameModule.cs
      PlayerController.cs
  .caligo/
    cache/
    compiled/
      Debug/
        MeuJogo.Gameplay.dll
        MeuJogo.Gameplay.pdb
        diagnostics.json
      last-good/
```

- O assembly é `MeuJogo.Gameplay.dll`, com o nome declarado em `gameplayAssembly`.
- A lista de fontes é todo `.cs` sob a pasta do projeto C# apontado por `gameplayProject`, em ordem ordinal, excluindo `bin/`, `obj/` e pastas ocultas.
- O `.csproj` existe para o IDE externo abrir o projeto com conforto. O editor não o interpreta e nunca chama MSBuild. Referências, defines e nome do assembly vêm do contrato da engine e do `.cproject`.
- O namespace raiz do assembly é o nome do assembly. O tipo de entrada é `<gameplayAssembly>.GameModule`; no exemplo, `MeuJogo.Gameplay.GameModule`. Ele implementa `Caligo.Scripting.IGameModule` e registra cada comportamento no `ScriptRegistry`.
- O assembly referencia apenas `Caligo.Scripting.dll` e a biblioteca base do .NET 10. Não referencia render, física, áudio ou memória. Não há pacote NuGet no jogo; a superfície de referência é fechada.
- O mesmo tipo de script não se repete na mesma entidade. O nome do tipo é o contrato gravado na cena.
- Dois projetos não compartilham assembly. Alternar de projeto troca a raiz de conteúdo, a cena inicial e o assembly (D-028).

O layout no artefato da engine:

```
caligo.exe
runtime/
  dotnet/            runtime .NET privado, usado pelo jogo e pelo compilador
  host/              Caligo.Scripting.Loader.dll e runtimeconfig
  gameplay/          Caligo.Scripting.dll
  compiler/          Caligo.Compiler.dll, Roslyn e pacote de referência
```

## 3. Modelo de script

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

- `OnCreate` roda quando o script é anexado no início da execução e de novo quando o assembly é recarregado.
- `OnUpdate` roda no tick fixo de 60 Hz.
- `OnDestroy` roda ao parar a execução, inclusive quando há erro, e antes de uma recarga.
- `Transform` é local à entidade, não o mundo.
- Uma entidade pode ter vários scripts, mas nunca o mesmo tipo repetido.
- Nomes de script começam com letra e usam apenas letras, dígitos e sublinhado; o nome é o contrato da cena.
- Estado estático mutável é proibido. O `AssemblyLoadContext` coletável só descarrega se não houver raiz estática viva; o analisador do compilador avisa quando um campo estático mutável é declarado no assembly de gameplay.

Entrada exposta nesta fatia: `W`, `A`, `S`, `D`, `Space`, `LeftShift`, setas e `Escape`, consultadas por `IsKeyDown` e `IsKeyPressed`. A entrada chega ao script como um snapshot do quadro, sem expor `platform` ao assembly gerenciado.

## 4. Compilação

### 4.1 Compilador embutido

- O artefato da engine leva o Roslyn na forma de assemblies, dentro de `runtime/compiler/`. Não há download, restore ou instalação no projeto do jogo.
- A compilação roda em um processo auxiliar, `Caligo.Compiler`, sobre o mesmo runtime .NET privado. O processo isola falha e vazamento do Roslyn do editor e da CLI.
- Quem compila — o editor ou a CLI — grava um pedido em `.caligo/compiled/request.json`: lista de fontes, assembly de saída, configuração, defines, referências e raiz do projeto. O auxiliar responde com `diagnostics.json` e o código de saída.
- As referências são fixas: o pacote de referência do .NET 10 que acompanha a engine e `Caligo.Scripting.dll`. Nenhuma referência é resolvida da máquina.
- Defines: `CALIGO` sempre; `DEBUG` na configuração Debug. Não há define por plataforma além de `WINDOWS` e `X64`, que a engine fornece.
- A compilação é determinística: `/deterministic`, ordem ordinal das fontes, `pathmap` da raiz do projeto e versão do compilador fixa. O mesmo código produz o mesmo assembly e o mesmo PDB.
- A saída vai para `.caligo/compiled/<config>/`. Só substitui o assembly anterior depois de uma compilação sem erro; o anterior vira `last-good` e permanece carregável.
- O build do jogo compila em Release e copia o assembly para `runtime/gameplay` no stage (BUILD §6).

### 4.2 Diagnóstico

Cada diagnóstico tem arquivo, linha, coluna, severidade, código e mensagem. O console do editor mostra com filtro e link para a linha; a barra de status mostra a contagem de erros e avisos. O painel de scripts lista os arquivos com erro.

- Erro impede o build e impede o início de uma nova execução.
- Aviso não impede nada e aparece no console.
- Compilar ao salvar é uma configuração do editor, ligada por padrão. Salvar um `.cs` compila; erro não abre diálogo modal e não derruba o editor.
- Se a compilação falhar durante uma execução ativa, a execução continua no último assembly válido e a barra de status marca "scripts desatualizados". O build sempre falha; nunca empacota assembly velho.
- F5 compila se houver fonte alterada desde a última compilação bem-sucedida. Erro impede o play; o usuário corrige ou desfaz.
- F8 compila e, se não houver erro, reinicia o contexto gerenciado e reanexa os scripts à cena em execução. Se houver erro, a execução atual continua e os diagnósticos aparecem.

### 4.3 Depuração

- Debug gera PDB portátil; Release não gera símbolo.
- O comando **Anexar depurador** mostra o processo, o assembly, o PDB e a raiz das fontes, e abre o IDE externo configurado. O editor não implementa depurador próprio.
- O IDE externo é opcional. Quem não o tem usa o console e os diagnósticos do editor.
- O anexo funciona no modo jogo e no play-in-editor; no play-in-editor, o processo é o do editor e o assembly é o compilado em Debug.
- Ponto de quebra, inspeção e passo são responsabilidade do IDE; a engine garante símbolo, caminho de fonte e processo estável.

## 5. Ciclo de execução

O play-in-editor trabalha sobre a cena aberta:

1. A cena inteira é copiada para um snapshot.
2. Os scripts são compilados, se necessário; erro impede o início.
3. O host gerenciado reinicia, carrega o assembly do projeto e instancia os scripts das entidades.
4. A simulação avança em tick fixo de 60 Hz, usando a câmera ativa do play; antes da R4, a câmera do editor.
5. Pausa congela o tempo; o avanço de quadro executa exatamente um tick.
6. Parar descarta o estado simulado e restaura o snapshot, sem contaminar a autoria.

Durante a execução a edição fica bloqueada: hierarquia, transformação, nomes, scripts e gizmo não alteram a cena. Salvar, abrir e criar cena também ficam indisponíveis até parar. A execução nunca grava no arquivo de cena.

Atalhos: `F5` inicia ou para, `F6` pausa ou continua, `F7` avança um quadro quando pausado e `F8` compila e recarrega os scripts. Recarregar durante a execução reinicia o contexto gerenciado e reanexa os scripts à cena atual; o estado da cena em execução permanece, mas o estado interno dos scripts recomeça.

## 6. Hospedagem nativa

- `hostfxr` é carregado de um runtime .NET privado em `<exe>/runtime/dotnet`.
- O bootstrap `Caligo.Scripting.Loader` não é coletável e expõe funções `[UnmanagedCallersOnly]`.
- O bootstrap cria um `AssemblyLoadContext` coletável, copia `Caligo.Scripting.dll` e o assembly do projeto para uma pasta temporária e carrega os dois nesse contexto.
- Cada início de execução descarrega o contexto anterior; o shadow copy evita bloquear os arquivos originais.
- O host nativo fica carregado no editor desde o início, para listar tipos no inspetor; só o contexto gerenciado é recriado.
- No editor, o assembly é localizado pelo `.cproject`: `gameplayAssembly` mais `.dll` em `.caligo/compiled/<config>/`. No pacote, o nome vem do `build.json` e o arquivo está em `runtime/gameplay`.
- Sem script, a engine inicia e roda a cena; o host só é exigido quando há assembly de gameplay.

## 7. Erros

Falha ao carregar o runtime, registrar um tipo, anexar um script ou executar um tick vira diagnóstico no console do editor e interrompe a execução quando necessário. Falha de compilação durante uma execução ativa mantém o último assembly válido, como descrito em 4.2. Um script que falha não deixa a simulação rodando em estado parcial: a cena volta ao snapshot.

- Tipo de entrada ausente: erro de carga, com o nome esperado e o assembly.
- Tipo de script citado na cena e ausente no assembly: erro de carga, com entidade e tipo.
- Assinatura incompatível: erro de compilação ou de carga, nunca comportamento indefinido.
- Exceção em `OnCreate`, `OnUpdate` ou `OnDestroy`: log com tipo, mensagem e pilha; a execução para e restaura o snapshot.
- Assembly corrompido ou versão de ABI incompatível: erro de carga, sem tentativa de interpretação.

## 8. Testes

- Testes gerenciados cobrem registro de tipos, nomes inválidos, anexação duplicada, delta inválido, rotação, movimento e normalização diagonal.
- Testes do compilador cobrem fonte válida, erro de sintaxe, erro de tipo, aviso de estado estático, determinismo do assembly, PDB em Debug e ausência de PDB em Release.
- O teste nativo cobre o host sem runtime disponível.
- O smoke de linha de comando compila o jogo de exemplo, carrega o runtime privado, inicia a execução, roda 60 ticks, confere o movimento e a rotação e encerra o host.
- O CI da engine roda os testes gerenciados, os testes do compilador e o smoke dentro do `ctest`, em Debug e Release. Esse CI valida a engine e o jogo de exemplo; não é requisito do build de um jogo.

## 9. O que entra em cada release

| Release | Scripting |
|---|---|
| R1 | assembly único, host privado, play-in-editor, F5 a F8 e recarga |
| R3 | assembly por projeto, Roslyn embutido, compilar ao salvar, diagnósticos, PDB e anexo externo; F8 compila |
| R4 | estado de animação e Tônus expostos ao script |
| R7 | recarga de assets durante o play e polimento do fluxo de depuração |

## 10. Fora de escopo na v1

- Visual scripting (D-013).
- NuGet, pacote de terceiros ou plugin no assembly de gameplay.
- SDK, MSBuild ou `dotnet build` como requisito do projeto do jogo.
- Depurador próprio dentro do editor.
- Compilação remota ou em nuvem.
- Hot reload de código nativo.

## 11. Histórico

- **0.2** — assembly por projeto, compilador Roslyn embutido em processo auxiliar, compilar ao salvar, último assembly válido, F8 com compilação, PDB e anexo por IDE externo, sem SDK/MSBuild/NuGet/CI no jogo.
- **0.1** — modelo de script, ciclo de play, hospedagem privada e testes da R1.
