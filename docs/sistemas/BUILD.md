# Caligo — Build e Entrega

Versão 0.1

## 1. Responsabilidade

Este documento define como um projeto Caligo vira um jogo executável: descritor de projeto, cozimento, compilação de scripts, montagem do pacote, execução em modo jogo e distribuição.

A regra central é a D-042: **produzir um jogo é local e autossuficiente**. Compilar, cozinhar e empacotar acontecem na máquina do desenvolvedor, dentro do editor ou pela linha de comando. Não depende de CI, rede ou GitHub. O CI existe apenas para desenvolver e validar a engine.

## 2. Princípios

- **Um caminho de build.** O botão do editor e o comando de linha chamam a mesma implementação.
- **Só cozido entra no pacote.** Nenhuma fonte de cena, modelo, textura ou script de gameplay.
- **Reprodutível.** Mesma fonte, mesmo projeto, mesma versão de engine e mesmas ferramentas produzem o mesmo conteúdo de pacote.
- **Um executável, dois modos.** Editor e jogo compartilham o binário (D-005). O pacote abre no modo jogo.
- **Autossuficiente.** Runtime .NET privado (D-027) e CRT estática; a máquina do jogador não instala SDK nem runtime.
- **Falha cedo.** Cook, compilação ou manifesto com erro não geram pacote parcial.
- **Sem surpresa de plataforma.** A v1 é Windows x64 (D-004, D-012); nenhum outro alvo é prometido.

## 3. Projeto

O projeto é um descritor de conteúdo (D-028), gravado em `MeuJogo.cproject`, JSON UTF-8 versionado. O contrato completo está em `docs/FORMATOS.md` §3.1.

Campos principais:

| Campo | Função |
|---|---|
| `name` | nome do projeto e do pacote |
| `projectVersion` | versão do jogo |
| `contentRoot` | raiz do conteúdo, padrão `content` |
| `initialScene` | cena de origem, relativa a `contentRoot`, que o cook resolve para o GUID inicial |
| `gameplayProject` | projeto C# do jogo |
| `gameplayAssembly` | nome do assembly gerado |
| `target` | `windows-x64` na v1 |
| `output` | pasta de saída do build |
| `render` | padrões de render do projeto |

O hub do editor cria, abre e alterna projetos. Cada projeto tem a própria raiz de conteúdo, a própria cena inicial e o próprio assembly; dois jogos não compartilham assembly (D-028).

Árvore padrão:

```
MeuJogo/
  MeuJogo.cproject
  content/
    scenes/
    assets/
    ui/
  scripts/
    MeuJogo.Gameplay/
  cooked/
  .caligo/
    cache/
    compiled/
```

`cooked/` e `.caligo/` são gerados. O controle de versão guarda a fonte (incluindo `.cterrain` e `.cui`), os sidecars `.cmeta`, a cena, o script e o `.cproject`.

## 4. Cozimento

O cook transforma fonte em `.casset` e grava o manifesto. Ele é incremental, determinístico e paralelo; o detalhamento está em `docs/sistemas/ASSETS.md` §8.

- Entrada: `content/`, configurações dos `.cmeta`, versão do importador e versão do cozinhador.
- Saída: `cooked/` com os assets e `cooked/manifest.casset` (tipo 9).
- Erro em qualquer asset falha o cook e lista todos os diagnósticos.
- O cook não lê configuração de usuário do editor.
- O cook não grava data, hora, caminho absoluto ou usuário.

Cozinhar não compila C++. A engine já está compilada; o jogo só compila o próprio assembly C#.

## 5. Build

O build é a soma de quatro etapas, na ordem:

1. **Cozinhar** o conteúdo.
2. **Compilar** o assembly de gameplay com o compilador embutido na engine (`docs/sistemas/SCRIPTING.md` §4).
3. **Montar** o stage com executável, runtime privado, conteúdo cozido, manifesto e metadados.
4. **Empacotar** em pasta e zip.

Comandos, no mesmo executável:

```
caligo.exe cook  <projeto>
caligo.exe build <projeto> --config Release --out <pasta>
caligo.exe run   <projeto>
caligo.exe --game <pacote>
```

- `cook` só cozinha.
- `build` cozinha, compila e monta.
- `run` abre o editor com o projeto.
- `--game` abre o modo jogo de um pacote, sem editor. A pasta precisa conter `build.json`; dentro do pacote, `caligo.exe` sem argumento faz o mesmo. `caligo.exe` sem argumento fora de um pacote abre o editor.
- `<projeto>` é a pasta que contém o `.cproject` ou o caminho do próprio descritor; `<pacote>` é a pasta do stage.
- `--out` vence o campo `output` do projeto.

O botão **Build** do editor chama `build` internamente. Não existe segundo caminho de empacotamento.

Configurações:

- `Debug` gera PDB do jogo e mantém o console de diagnóstico.
- `Release` não leva PDB e usa otimização.
- `--no-cook` reaproveita o cozido existente, para iteração de script.
- `--cook-only` para quando só o conteúdo mudou.

## 6. Stage e pacote

Layout do stage:

```
MeuJogo/
  caligo.exe
  build.json
  runtime/
    dotnet/            runtime .NET privado
    host/              bootstrap do host de script
    gameplay/          Caligo.Scripting.dll e MeuJogo.Gameplay.dll
  cooked/
    manifest.casset
    ...                assets cozidos
  LICENSE
  THIRD-PARTY-NOTICES
```

Regras:

- `runtime/dotnet`, `runtime/host` e `runtime/gameplay` vêm do artefato da engine e do build do projeto (D-027).
- `runtime/gameplay` contém `Caligo.Scripting.dll`, do artefato da engine, e `<gameplayAssembly>.dll`, do build do projeto.
- `cooked/` contém apenas `.casset` e o manifesto. Nenhuma fonte.
- O `.cproject` é fonte de autoria e **não entra no pacote**. O runtime não lê caminho de fonte.
- `build.json` contém `format` e `version` do próprio schema, nome, `projectVersion`, versão da engine, versão do formato cozido, build ID, hash do manifesto, GUID da cena inicial e nome do assembly. Não contém data nem hora.
- O pacote não leva `runtime/compiler`, `.csproj`, `.cs`, `.cscene`, `.gltf`, `.png` nem qualquer fonte de conteúdo.
- `LICENSE` e `THIRD-PARTY-NOTICES` acompanham o pacote com as dependências efetivamente distribuídas no binário e no runtime. O compilador Roslyn não é distribuído no pacote do jogo; o aviso dele acompanha o artefato da engine.
- PDB só existe em `Debug`; `Release` não distribui símbolo.
- Logs e relatório de crash vão para `%LOCALAPPDATA%/Caligo/<Projeto>/logs`, nunca para a pasta de instalação.

O pacote final é a pasta do stage compactada em `<Projeto>-<projectVersion>-windows-x64.zip`.

## 7. Modo jogo

O mesmo executável abre em dois modos:

| Modo | Entrada | Comportamento |
|---|---|---|
| Editor | `caligo.exe run <projeto>` na instalação da engine | abre o editor, ferramentas e console |
| Jogo | `caligo.exe --game <pacote>` ou `caligo.exe` dentro do pacote (com `build.json` ao lado) | abre a cena inicial, sem editor, sem ferramentas, sem console de edição |

No modo jogo:

- A cena inicial vem do GUID gravado no `build.json` e resolvido pelo manifesto.
- O assembly de gameplay vem do `build.json` e de `runtime/gameplay`.
- O conteúdo vem do manifesto cozido.
- O input do jogador é o do jogo, não o do editor.
- `Esc` abre o menu de pausa do jogo, se o jogo tiver UI; não abre o editor.
- Salvar cena, abrir cena e gizmo não existem.
- O pacote não leva `runtime/compiler` nem ferramentas de edição; o modo editor exige a instalação da engine.

A D-005 mantém um único executável. Se um jogo público precisar de binário sem código de editor, isso será uma opção de compilação do mesmo código, não uma segunda engine; fica para depois da v1.

## 8. Determinismo

- O build ID é o hash do manifesto mais a versão do projeto, a versão da engine e a versão do compilador.
- Nenhum carimbo de tempo entra no pacote, no manifesto ou no `build.json`.
- A ordem dos arquivos no zip é estável.
- A compressão usa nível fixo.
- O compilador C# é o que acompanha a engine; a versão entra no build ID.
- Dois builds da mesma fonte com as mesmas ferramentas produzem o mesmo hash de conteúdo. O zip pode diferir em metadados do arquivador; o conteúdo, não.

## 9. Plataforma

A v1 é **Windows 10 21H2+ e Windows 11, x64**, Direct3D 11 FL11_0 (D-004). Não há promessa de outra plataforma (D-012).

- O `CMakeLists.txt` recusa sistema que não seja Windows.
- O pacote é `windows-x64`.
- DX12 não entra na v1; a RHI nasce preparada, mas a entrega é D3D11.
- Port é projeto pós-v1, com backend gráfico, camada de plataforma, áudio, empacotamento, runtime .NET e imagens douradas próprias. Não é uma flag.
- O formato cozido é little-endian, sem ponteiro, sem caminho absoluto e sem dependência de API; os dados sobrevivem a um port por consequência do formato, não por promessa de port.
- Console, macOS, mobile, VR e rede continuam fora (D-012).

## 10. Distribuição na v1

Entra:

- Pasta executável e zip.
- Runtime privado e CRT estática.
- Log local e relatório de crash.
- Licença e avisos de terceiros.

Não entra na v1:

- Instalador.
- Auto-atualização.
- Loja (Steam, Epic, Microsoft Store).
- Assinatura de código.
- DLC, mods e workshop.
- Telemetria.
- Serviço de crash remoto.

Essas camadas são comerciais, entram quando um jogo exigir, e não mudam o formato do pacote.

Atualização futura usa o manifesto: baixar apenas assets e assembly cujo hash mudou, não o pacote inteiro. Fica para depois da v1.

## 11. Provas

No CI da engine, com o projeto de exemplo:

- Build completo em Debug e Release.
- Cook determinístico: dois cooks produzem o mesmo manifesto.
- Execução headless do stage por 120 quadros.
- Verificação de que nenhum arquivo-fonte entrou no pacote.
- Verificação de que todo GUID do manifesto resolve.
- Verificação de que `runtime/dotnet`, `runtime/host` e `runtime/gameplay` existem.

Na máquina de desenvolvimento:

- Rodar o stage em uma máquina sem .NET SDK e sem Visual C++ Redistributable.
- Abrir o jogo, entrar na cena inicial, sair e conferir o log.

O CI da engine valida a engine e o jogo de exemplo. Ele não é requisito para o build de um jogo.

## 12. O que entra em cada release

| Release | Build e entrega |
|---|---|
| R3 | hub de projetos; cook, compilação, stage, `build.json` e zip do jogo de exemplo |
| R5 | build interno para o playtest da cidade |
| R7 | pacote de produção, validação de stage e distribuição |
| R8 | distribuição da fatia vertical e endurecimento |

## 13. Fora de escopo na v1

- Instalador, updater e loja.
- Assinatura de código.
- Build para outra plataforma.
- Build na nuvem como requisito.
- Pacote monolítico fechado; a pasta com manifesto resolve a v1.

## 14. Histórico

- **0.1** — define projeto, cook, build local, stage, modo jogo, pacote zip, determinismo e o recorte de plataforma da v1.
