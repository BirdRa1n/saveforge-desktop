# SaveForge Desktop

Aplicativo desktop open source para encontrar, fazer backup e restaurar saves de jogos de PC. Funciona com Steam, Epic, GOG e instalações manuais, **totalmente offline**.

> **Estado:** em desenvolvimento inicial (0.1.0). Já estão prontos a estrutura do projeto, a interface base, a resolução de pastas do sistema (incluindo prefixos Wine/Proton) e a leitura, validação e resolução de perfis de jogos. Detecção de jogos, backup e restauração continuam na Fase 1 do [roadmap](../saveforge-docs/planejamento/PLANEJAMENTO-TECNICO.md#18-roadmap-com-dependências).

## Stack

- C++20 e Qt 6.8+ (Qt Quick / QML), com builds oficiais na versão estável mais recente
- CMake ≥ 3.24 com presets e Ninja
- Visual: direção **Forja** (`saveforge-docs/design/FORJA.md`)

## Estrutura

```
src/
  core/      tipos de domínio, Result/Error, versão (só QtCore)
  platform/  pastas do usuário por SO e dentro de prefixos Wine/Proton
  profiles/  perfis de jogos: parser v1, regras de segurança de caminhos, resolução de arquivos
  app/       executável GUI + módulo QML SaveForge.Ui (tema, componentes, páginas)
  cli/       saveforge-cli (sem GUI)
tests/
  unit/      testes de unidade e de conformidade com as specs (Qt Test)
  ui/        teste de fumaça da interface (falha em qualquer aviso de QML)
external/
  specs/     submódulo saveforge-specs (schemas, catálogos e vetores de teste)
```

Novos módulos (`db`, `detection`, `vault`, `archive`, `registry`, `services`) entram conforme o roadmap.

## Compilar

As especificações ficam em um submódulo. Depois de clonar, inicialize-o:

```bash
git submodule update --init
```

Aponte o `CMAKE_PREFIX_PATH` para a sua instalação do Qt.

```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
```

```bash
cmake --build --preset debug
```

```bash
ctest --preset debug
```

No Windows com MinGW, o `PATH` precisa incluir `C:/Qt/Tools/mingw1310_64/bin`, `C:/Qt/Tools/Ninja` e `C:/Qt/6.12.0/mingw_64/bin`. Também é possível abrir a pasta no Qt Creator, que detecta os presets.

O executável gerado é `out/build/debug/SaveForge(.exe)`. Para verificar o QML com o qmllint:

```bash
cmake --build --preset debug --target all_qmllint
```

Para salvar capturas de cada página da interface, defina uma pasta e rode o teste de fumaça:

```bash
SAVEFORGE_SCREENSHOT_DIR=/caminho/para/pasta ./out/build/debug/tst_ui_smoke
```

## Licença

MIT. Veja [LICENSE](LICENSE).
