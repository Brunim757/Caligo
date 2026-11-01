# Caligo

Engine de mundo aberto estilizado da Kizuri Studio.

- Escopo: [docs/ESCOPO.md](docs/ESCOPO.md)
- Arquitetura: [docs/ARQUITETURA.md](docs/ARQUITETURA.md)
- Formatos: [docs/FORMATOS.md](docs/FORMATOS.md)
- Decisões: [docs/DECISOES.md](docs/DECISOES.md)
- UI de jogo: [docs/sistemas/UI.md](docs/sistemas/UI.md)

## Licença

Proprietário. Todos os direitos reservados. A visibilidade pública existe apenas para o uso do GitHub Actions; nenhuma licença é concedida. Consulte [LICENSE](LICENSE).

## Estado

R0 — fundação. Núcleo, plataforma, janela, input, log, tempo, jobs, testes e modo headless.

R1 — editor e cena, em andamento. Esta fatia entrega o editor com hierarquia, inspetor de transformação, viewport com gizmo, console, abertura e gravação de `.cscene`, tema adaptado a DPI e uma cena de exemplo. Scripting C#, play-in-editor e cook entram nas próximas fatias da R1.

## Construir

Windows 10/11 x64, Visual Studio 2022, CMake 3.24+, Ninja e Python 3.13.

```powershell
python -m pip install clang-format==21.1.8
python tools/check_source.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
build/bin/Debug/caligo.exe --headless --frames 120
```

## Executar

`build/bin/Debug/caligo.exe` abre o editor com `content/scenes/exemplo.cscene`. `--scene <caminho>` abre outra cena e `--headless --frames 120` roda a verificação sem janela.

No viewport, o botão direito orbita, o botão do meio desloca e a roda aproxima. W, E e R alternam mover, girar e escalar; F enquadra a seleção e Ctrl encaixa o gizmo. O menu Arquivo abre e grava cenas, e o menu Exibir restaura o layout.

O GitHub Actions publica os artefatos `caligo-windows-Debug` e `caligo-windows-Release` com o executável, `content/` e a licença.
