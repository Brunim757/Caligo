# Caligo

Engine de mundo aberto estilizado da Kizuri Studio.

- Escopo: [docs/ESCOPO.md](docs/ESCOPO.md)
- Arquitetura: [docs/ARQUITETURA.md](docs/ARQUITETURA.md)
- Formatos: [docs/FORMATOS.md](docs/FORMATOS.md)
- Decisões: [docs/DECISOES.md](docs/DECISOES.md)
- Manual: [docs/manual/README.md](docs/manual/README.md)
- Scripting e play: [docs/sistemas/SCRIPTING.md](docs/sistemas/SCRIPTING.md)
- UI de jogo: [docs/sistemas/UI.md](docs/sistemas/UI.md)

## Licença

Proprietário. Todos os direitos reservados. A visibilidade pública existe apenas para o uso do GitHub Actions; nenhuma licença é concedida. Consulte [LICENSE](LICENSE).

## Estado

R0 — fundação. Núcleo, plataforma, janela, input, log, tempo, jobs, testes e modo headless.

R1 — a engine que se usa. Editor com hierarquia, inspetor, viewport com gizmo e console; cena com entidades e hierarquia em `.cscene`; componentes de script em C#; play-in-editor com tick fixo, pausa, avanço de quadro, recarga e restauração por instantâneo; gravação e abertura com proteção contra perda; tema adaptado a DPI, cena de exemplo e manual.

A próxima fatia é a R2 — imagem: render graph, luz, sombra, TAA, pós-processamento, áudio base e testes de imagem.

## Construir

Windows 10/11 x64, Visual Studio 2022, CMake 3.24+, Ninja, Python 3.13 e .NET SDK 10.

```powershell
python -m pip install clang-format==21.1.8
python tools/check_source.py
dotnet run --project scripts/Caligo.Gameplay.Tests/Caligo.Gameplay.Tests.csproj -c Debug
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
build/bin/Debug/caligo.exe --headless --frames 120
```

## Executar

`build/bin/Debug/caligo.exe` abre o editor com `content/scenes/exemplo.cscene`. `--scene <caminho>` abre outra cena, `--headless --frames 120` roda a verificação sem janela e `--script-smoke` valida o runtime gerenciado.

No viewport, o botão direito orbita, o botão do meio desloca e a roda aproxima. W, E e R alternam mover, girar e escalar; F enquadra a seleção e Ctrl encaixa o gizmo. O menu Arquivo abre e grava cenas, o menu Executar inicia a simulação e o menu Exibir restaura o layout.

F5 inicia ou para a execução, F6 pausa, F7 avança um quadro e F8 recarrega os scripts. Durante a execução a cena não pode ser editada; parar restaura o snapshot.

O GitHub Actions publica os artefatos `caligo-windows-Debug` e `caligo-windows-Release` com o executável, `runtime/`, `content/` e a licença.
