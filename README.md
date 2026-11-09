# SaveForge Desktop

Aplicativo desktop open source para encontrar, fazer backup e restaurar saves de jogos de PC. Funciona com Steam, Epic, GOG e instalações manuais, **totalmente offline**.

> **Estado:** em desenvolvimento inicial (0.1.0). O núcleo da Fase 1 do [roadmap](../saveforge-docs/planejamento/PLANEJAMENTO-TECNICO.md#18-roadmap-com-dependências) já funciona, pela interface e pela linha de comando:
> - perfis de jogos vindos do Registry assinado (22 mil jogos via Ludusavi): baixados dos mirrors, embutidos no build ou importados de uma pasta, sempre verificados e guardados em cache para uso offline;
> - detecção de jogos (Steam, Epic, GOG e Heroic, com prefixos Wine/Proton, além da busca por saves de jogos não instalados);
> - backup no vault local (deduplicação, compressão, verificação de integridade);
> - restauração segura (plano prévio, ponto de desfazer, troca atômica, política de cloud do launcher).
>
> Ainda faltam as chaves de produção e um endereço oficial do Registry, o registro do Windows, os backups automáticos e a exportação `.gamesave`.

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
  vault/     vault local: blobs por conteúdo (SHA-256 + zstd), snapshots, verificação e coleta de lixo
  detection/ detecção de jogos instalados (Steam, Epic, GOG, Heroic) e associação a perfis
  registry/  Registry: verificação Ed25519, download dos mirrors, cache local dos canais e catálogo de perfis
  services/  casos de uso: biblioteca, backup e restauração (checagem de jogo aberto, plano, desfazer)
  backend/   módulo QML SaveForge.Backend: controlador, modelos de lista e configurações do app
  app/       executável GUI + módulo QML SaveForge.Ui (tema, componentes, páginas; só apresentação)
  cli/       saveforge-cli (sem GUI)
tests/
  unit/      testes de unidade e de conformidade com as specs (Qt Test)
  support/   fixtures compartilhados entre testes
  data/      dados de teste (distribuições do Registry assinadas com chaves de teste)
  ui/        teste de fumaça da interface (falha em qualquer aviso de QML)
external/
  specs/     submódulo saveforge-specs (schemas, catálogos e vetores de teste)
```

Novos módulos (`db`, `archive`) entram conforme o roadmap.

Decisões locais deste repositório ficam em [`docs/adr/`](docs/adr/README.md).

## Compilar

As especificações ficam em um submódulo. Depois de clonar, inicialize-o:

```bash
git submodule update --init
```

Aponte o `CMAKE_PREFIX_PATH` para a sua instalação do Qt. Se a biblioteca **zstd** não estiver instalada no sistema, o primeiro configure baixa a versão fixada e confere o SHA-256 antes de compilar (veja [ADR 0001](docs/adr/0001-third-party-dependencies.md)). Para compilar offline, instale o zstd ou aponte `FETCHCONTENT_SOURCE_DIR_ZSTD` para o código-fonte.

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

Para capturar a interface com dados reais (biblioteca escaneada, página de um jogo e diálogo de restauração), aponte também `SAVEFORGE_DATA_DIR` para uma pasta de dados preparada, por exemplo com um Registry importado, e escolha um jogo com `SAVEFORGE_SCREENSHOT_GAME=<id>`. O teste fará um backup desse jogo no vault dessa pasta antes de capturar.

## Linha de comando

`saveforge-cli` expõe as funções do app sem interface gráfica. Use `saveforge-cli <comando> --help` para ver todas as opções.

| Comando | O que faz |
|---|---|
| `registry update [--mirror <url>]` | Baixa os canais dos mirrors (os do build ou os de `--mirror`), verifica e guarda no cache. Não baixa de novo o que já está em cache. |
| `registry import <pasta>` | Verifica uma distribuição do Registry (assinaturas, rollback, hashes) e a guarda no cache local. |
| `registry status` | Mostra os canais em cache, o total de perfis e as atribuições de licença. |
| `detect [--profiles <pasta>]` | Lista os jogos instalados, associa-os aos perfis (do cache ou da pasta) e procura saves de jogos não instalados. Só leitura. |
| `backup --vault <pasta> --game <id>` | Cria um snapshot dos saves do jogo. |
| `snapshots --vault <pasta>` | Lista os snapshots do vault. |
| `restore --vault <pasta> --game <id> --snapshot <id>` | Mostra o plano de restauração. Só escreve com `--yes`. |

Em `backup` e `restore`, `--profile <arquivo.json>` substitui `--game` para usar um perfil fora do Registry.

**Registry em desenvolvimento.** Enquanto não existem chaves de produção, o app confia nas chaves de teste **públicas** do `saveforge-specs`. O configure do CMake e a CLI avisam sobre isso. Para gerar uma distribuição local, veja o README do `saveforge-registry`. Builds de release devem definir `SAVEFORGE_REGISTRY_TRUST_ANCHOR`.

**De onde vêm os perfis.** Veja a [ADR 0003](docs/adr/0003-registry-download.md). Três opções do CMake controlam isso:

| Opção | Efeito |
|---|---|
| `SAVEFORGE_REGISTRY_TRUST_ANCHOR` | Root do Registry em que o build confia. |
| `SAVEFORGE_REGISTRY_MIRRORS` | Mirrors padrão, separados por `;`: URLs HTTPS ou pastas locais. Com mirrors, o app procura novos perfis uma vez por dia, e isso pode ser desligado em Configurações. A requisição leva só a versão do app. |
| `SAVEFORGE_REGISTRY_BUNDLED_DIR` | Pasta de uma distribuição (o `dist/v1` do `saveforge-registry`) embutida no app. Ela alimenta o cache na primeira execução, para que os perfis funcionem sem internet. |

Por padrão as opções de mirrors e de distribuição embutida ficam vazias. Para testar o download localmente, gere uma distribuição no `saveforge-registry` e rode `saveforge-cli registry update --mirror <pasta-ou-url>`. Também é possível definir `mirrors=<url>` no grupo `[registry]` do `settings.ini`.

Os dados do app (cache do Registry) ficam em `%LOCALAPPDATA%/SaveForge` no Windows, `~/.local/share/SaveForge` no Linux e `~/Library/Application Support/SaveForge` no macOS. A variável `SAVEFORGE_DATA_DIR` permite trocar esse local.

Na restauração:
- sempre é criado antes um snapshot `pre-restore`, que permite desfazer a operação;
- arquivos que existem hoje e não estão no snapshot são mantidos;
- com o jogo aberto, a restauração é recusada;
- se o launcher também sincroniza os saves pelo próprio cloud, é preciso escolher `--cloud prefer-saveforge` ou `--cloud prefer-launcher`.

Para testar sem tocar nos seus saves reais, use `--map <placeholder>=<pasta>`, que redireciona uma pasta raiz (por exemplo `--map winAppData=D:/sandbox`).

## Licença

MIT. Veja [LICENSE](LICENSE).
