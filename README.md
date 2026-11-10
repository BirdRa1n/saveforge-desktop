# SaveForge Desktop

Aplicativo desktop open source para encontrar, fazer backup e restaurar saves de jogos de PC. Funciona com Steam, Epic, GOG e instalações manuais, **totalmente offline**.

> **Estado:** em desenvolvimento (0.2.0, alpha). As Fases 1 e 2 do [roadmap](../saveforge-docs/planejamento/PLANEJAMENTO-TECNICO.md#18-roadmap-com-dependências) já funcionam, quase completas, pela interface e pela linha de comando:
> - perfis de jogos vindos do Registry assinado (22 mil jogos via Ludusavi): baixados dos mirrors, embutidos no build ou importados de uma pasta, sempre verificados e guardados em cache para uso offline;
> - detecção de jogos (Steam, Epic, GOG, Heroic e Lutris, com prefixos Wine/Proton, além da busca por saves de jogos não instalados);
> - backup no vault local (deduplicação, compressão, verificação de integridade);
> - restauração segura (plano prévio, ponto de desfazer, troca atômica, política de cloud do launcher);
> - jogos adicionados à mão e perfis locais, que têm prioridade sobre o Registry;
> - saves com nome, entre os quais se alterna sem perder o progresso de nenhum, e backups automáticos (ao fechar o jogo ou periódicos), com o app na bandeja do sistema;
> - exportação e importação de arquivos `.gamesave` (completa ou incremental) para levar saves a outro computador.
>
> Ainda faltam as chaves de produção do Registry e os saves no registro dentro de prefixos Wine/Proton.

## Stack

- C++20 e Qt 6.8+ (Qt Quick / QML), com builds oficiais na versão estável mais recente
- CMake ≥ 3.24 com presets e Ninja
- Visual: direção **Console × Studio** (`saveforge-docs/design/CONSOLE-STUDIO.md`): tema claro, escuro ou do sistema, cor de destaque escolhida em Configurações › Aparência, e atalhos `Ctrl+K` (buscar), `Ctrl+B` (backup), `Ctrl+R` (restaurar) e `Ctrl+T` (trocar save)

## Estrutura

```
src/
  core/      tipos de domínio, Result/Error, versão (só QtCore)
  platform/  pastas do usuário por SO e dentro de prefixos Wine/Proton
  profiles/  perfis de jogos: parser v1, regras de segurança de caminhos, resolução de arquivos
  vault/     vault local: blobs por conteúdo (SHA-256 + zstd), snapshots, verificação e coleta de lixo
  archive/   arquivos .gamesave: leitura com todas as verificações da spec e escrita verificada
  detection/ detecção de jogos instalados (Steam, Epic, GOG, Heroic, Lutris) e associação a perfis
  registry/  Registry: verificação Ed25519, download dos mirrors, cache local dos canais e catálogo de perfis
  services/  casos de uso: biblioteca, perfis locais e jogos manuais, backup e restauração (checagem de jogo aberto, plano, desfazer)
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

O módulo `db` (índice SQLite) entra conforme o roadmap.

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

## Pacotes

Os pacotes são gerados com o CPack a partir de um build de release. No Windows saem um instalador (`-setup.exe`, feito com o Inno Setup 6 quando ele está instalado) e um `.zip` portátil; no Linux, um `.tar.gz` com a entrada `.desktop` e o ícone; no macOS, um `.dmg`. O instalador não pede administrador, porque instala só para o usuário atual, mas oferece instalar para todos. Ele também cria os atalhos e o desinstalador. A etapa de instalação copia para junto do app as bibliotecas, os plugins e os módulos QML do Qt, e por isso o pacote roda sem o Qt instalado.

```bash
cmake --preset release -DCMAKE_PREFIX_PATH=<caminho-do-qt> -DSAVEFORGE_BUILD_TESTS=OFF -DSAVEFORGE_REGISTRY_BUNDLED_DIR=<saveforge-registry>/dist/v1
```

```bash
cmake --build --preset release
```

```bash
cd out/build/release && cpack
```

`SAVEFORGE_PACKAGE_VERSION` muda a versão no nome do arquivo, por exemplo para `0.1.0-alpha.1`. O workflow `Release` gera os três pacotes, já com o Registry embutido. Ele roda manualmente, guardando os pacotes como artefatos, ou com uma tag `v<versão>`, que publica um pre-release no GitHub com as notas de [`docs/release-notes.md`](docs/release-notes.md). Os pacotes ainda não são assinados (tarefa D2.7 do roadmap).

## Linha de comando

`saveforge-cli` expõe as funções do app sem interface gráfica. Use `saveforge-cli <comando> --help` para ver todas as opções.

| Comando | O que faz |
|---|---|
| `export --vault <pasta> --output <arquivo> [--game <id>] [--all-history] [--incremental]` | Grava os backups num arquivo `.gamesave`. Sem `--game`, exporta todos os jogos. |
| `import --vault <pasta> <arquivo.gamesave>` | Adiciona ao vault os backups de um `.gamesave`, depois de verificar o arquivo inteiro. Não restaura nada. |
| `profiles add --name <nome> --folder <pasta>` | Adiciona à mão um jogo que o Registry não conhece, a partir das pastas de saves. `--folder` pode se repetir. |
| `profiles list` / `profiles import <arquivo.json>` / `profiles remove <id>` | Lista, importa ou remove perfis locais. Os backups de um jogo removido continuam no vault. |
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

Em builds de desenvolvimento, que confiam nas chaves de teste, o mirror padrão é a distribuição de teste que o `saveforge-registry` publica em `https://birdra1n.github.io/saveforge-registry/v1/`. Builds com outro trust anchor começam sem mirror e devem definir o próprio. A distribuição embutida fica vazia por padrão. Para testar com uma distribuição local, rode `saveforge-cli registry update --mirror <pasta-ou-url>`. Também é possível definir `mirrors=<url>` no grupo `[registry]` do `settings.ini`.

**Jogos adicionados à mão.** Viram perfis locais (`profile.v1`, com origem `local`), guardados em `<dados do app>/profiles/<id>.json`, e passam pela mesma validação dos perfis do Registry. Perfis descrevem locais por placeholders, nunca por caminhos absolutos. Por isso, as pastas de saves precisam ficar dentro das pastas do usuário (pasta pessoal, Documentos, AppData e equivalentes no Linux e no macOS). Pastas dentro da instalação do jogo ainda não são aceitas.

**Saves com nome.** Na página de um jogo, "Salvar como novo save" dá um nome ao progresso atual, por exemplo outro personagem. "Continuar este save" troca para outro save, depois de mostrar o plano de restauração. O progresso do save que fica para trás é guardado nele, e os backups seguintes vão para o save em uso. O botão "Abrir pasta dos saves" abre a pasta do jogo no gerenciador de arquivos. Veja a [ADR 0004](docs/adr/0004-save-slots-and-automatic-backups.md).

**Backups automáticos.** O app faz backup quando um jogo fecha e, se você quiser, a cada 1, 6 ou 24 horas, mas só quando os saves mudaram. Fechar a janela deixa o app na bandeja do sistema para os backups continuarem; isso pode ser desligado em Configurações.

**Importar e exportar.** A página "Importar / Exportar" grava os backups dos jogos escolhidos num arquivo `.gamesave`, que é um ZIP comum (formato em `saveforge-specs/gamesave`). Dá para levar só o backup mais recente ou todo o histórico, e para fazer uma exportação incremental, que leva só o que mudou desde a anterior. Importar adiciona os backups ao histórico depois de verificar o arquivo inteiro e nunca sobrescreve saves: a restauração continua sendo feita na página de cada jogo. O caminho das pastas do usuário só entra no arquivo se você pedir. Veja a [ADR 0005](docs/adr/0005-miniz-for-gamesave-archives.md).

**Proteções extras do perfil.** Se os saves de um jogo forem maiores do que o perfil espera (`backup.maxBytes`), o backup manual pede confirmação, porque isso pode indicar arquivos que não são saves. O backup automático pula o jogo nesse caso. Arquivos que o jogo grava juntos (`backup.consistentGroups`, como um banco de dados e o seu journal) são lidos de novo, todos juntos, se algum mudar durante a cópia.

**Espaço e manutenção.** Em Configurações, dá para guardar todos os backups de cada jogo ou só os 10, 30 ou 100 mais recentes, e opcionalmente tudo o que tiver menos de 30 dias, 90 dias ou 1 ano. Nunca são apagados os backups fixados (o ícone de alfinete no histórico), o estado atual de cada save com nome nem o ponto de desfazer da última restauração. Toda semana o app verifica o vault (cada arquivo contra o seu hash) e libera o espaço dos arquivos que nenhum backup usa. Os botões "Limpar agora" e "Verificar o vault" fazem isso na hora.

**Registro do Windows.** Perfis com regras de registro (chaves em `HKCU\Software`) têm essas chaves salvas junto com os arquivos, incluindo as subchaves, quando o jogo roda no Windows. Na restauração, o plano mostra quantas chaves serão restauradas; em outro sistema, elas são puladas e o plano avisa. Registros dentro de prefixos Wine/Proton ainda não são lidos.

**Capas dos jogos.** O app usa as imagens que os launchers já guardam no computador: a capa, o banner e o logo do cache da Steam (`appcache/librarycache`) e o ícone que a GOG instala junto do jogo. Jogos com ID da Steam que não têm capa local, como os não instalados, podem ter a capa baixada do CDN de imagens da Steam. O pedido só informa o ID do jogo, e o download pode ser desligado em Configurações. As capas baixadas ficam em `<dados do app>/covers`.

Os dados do app (cache do Registry, perfis locais, capas e configurações) ficam em `%LOCALAPPDATA%/SaveForge` no Windows, `~/.local/share/SaveForge` no Linux e `~/Library/Application Support/SaveForge` no macOS. A variável `SAVEFORGE_DATA_DIR` permite trocar esse local.

Na restauração:
- sempre é criado antes um snapshot `pre-restore`, que permite desfazer a operação;
- arquivos que existem hoje e não estão no snapshot são mantidos;
- com o jogo aberto, a restauração é recusada;
- se o launcher também sincroniza os saves pelo próprio cloud, é preciso escolher `--cloud prefer-saveforge` ou `--cloud prefer-launcher`.

Para testar sem tocar nos seus saves reais, use `--map <placeholder>=<pasta>`, que redireciona uma pasta raiz (por exemplo `--map winAppData=D:/sandbox`).

## Licença

MIT. Veja [LICENSE](LICENSE).
