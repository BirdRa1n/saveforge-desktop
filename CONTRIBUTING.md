# Contribuindo com o SaveForge Desktop

Obrigado por querer contribuir! Este guia vale para pessoas e para agentes de IA. Leia antes de abrir um pull request.

## Sumário

- [Antes de começar](#antes-de-começar)
- [Ambiente de desenvolvimento](#ambiente-de-desenvolvimento)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Convenções de código](#convenções-de-código)
- [Testes](#testes)
- [Commits](#commits)
- [Pull requests](#pull-requests)
- [Documentação](#documentação)

## Antes de começar

- **Arquitetura e decisões:** a arquitetura do ecossistema está em `saveforge-docs/planejamento/PLANEJAMENTO-TECNICO.md`, e as decisões estruturais estão nas ADRs, em `saveforge-docs/adr/`. Mudanças que contrariam uma ADR começam com uma nova ADR, não com código.
- **Visual:** a interface segue a direção **Forja** (`saveforge-docs/design/FORJA.md`).
- **Formatos de arquivo:** formatos compartilhados (perfis de jogos, `.gamesave`, Registry) são definidos em `saveforge-specs`. Não invente formatos dentro deste repositório.

## Ambiente de desenvolvimento

Requisitos:
- **Qt 6.8 ou mais recente**, com os módulos Core, Gui, Qml, Quick, QuickControls2 e Test.
- **CMake ≥ 3.24** e **Ninja**.
- Um compilador com suporte a C++20: MSVC 2022, GCC ≥ 12, Clang/AppleClang ≥ 15 ou MinGW 13.

```bash
git submodule update --init
```

```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=<caminho-do-qt>
```

```bash
cmake --build --preset debug
```

```bash
ctest --preset debug
```

```bash
cmake --build --preset debug --target all_qmllint
```

O preset `ci` trata avisos como erros. Rode-o antes de abrir um PR.

## Estrutura do projeto

```
src/
  core/        Tipos de domínio, Result/Error, versão. Depende apenas de QtCore.
  platform/    Pastas do usuário por SO e dentro de prefixos Wine/Proton.
  profiles/    Perfis de jogos: parser, regras de segurança de caminhos, resolução de arquivos.
  vault/       Vault local: blobs, snapshots, verificação e coleta de lixo.
  detection/   Detectores de launchers, associação de instalações a perfis, varredura save-first.
  registry/    Verificação do Registry (Ed25519, cadeia de root, rollback), cache e catálogo.
  services/    Casos de uso que combinam os módulos: biblioteca, backup e restauração.
  backend/     Módulo QML SaveForge.Backend: AppController (singleton), modelos e configurações.
  app/         Executável GUI + módulo QML SaveForge.Ui
    qml/theme/       Tokens visuais (Theme) e ícones (Icons) — singletons
    qml/components/  Componentes reutilizáveis, sem lógica de negócio
    qml/pages/       Telas completas, compostas a partir dos componentes
  cli/         saveforge-cli (sem GUI)
tests/
  unit/        Testes de unidade (Qt Test), um arquivo por módulo/assunto
  support/     Fixtures compartilhados (headers incluídos pelos testes)
  data/        Dados de teste versionados; registry/generate.mjs regenera as distribuições assinadas
  ui/          Testes da interface QML
cmake/         Módulos CMake auxiliares (avisos, dependências de terceiros)
docs/adr/      Decisões arquiteturais locais deste repositório
external/specs Submódulo saveforge-specs (não edite aqui; mude no repositório de specs)
.github/       Workflows de CI
```

**Specs como fonte única.** Dados definidos pelas specs (catálogo de placeholders, lista de locais sensíveis) são embutidos a partir de `external/specs` como recursos Qt, e os testes de conformidade leem os exemplos e vetores de lá. Uma mudança de comportamento definida pela spec começa no `saveforge-specs`. Depois, atualize o submódulo aqui (`git -C external/specs pull`) em um commit próprio.

**Regra de dependência entre módulos.** As setas indicam "depende de" e só existem nessa direção:

```
app, cli → services → detection, vault, archive, registry, db → profiles → platform → core
```

Na prática, o `registry` verifica e entrega perfis, e quem escolhe qual perfil usar para cada jogo é o catálogo (`ProfileCatalog`).

- Cada módulo é uma biblioteca estática (`saveforge::<módulo>`), com cabeçalhos públicos em `src/<módulo>/include/saveforge/<módulo>/`.
- Nada abaixo de `app` pode depender de QtGui ou QtQuick.
- Um módulo novo entra com seu `CMakeLists.txt`, os testes correspondentes e uma linha em `src/CMakeLists.txt`.

## Convenções de código

### C++
- C++20, sem extensões de compilador.
- Namespace `saveforge` (e `saveforge::<módulo>` quando fizer sentido).
- Nomes:
  - `PascalCase` para tipos;
  - `camelCase` para funções e variáveis;
  - `m_` como prefixo de membros privados;
  - `kPascalCase` para constantes.
- Erros são retornados como `Result<T>` / `Result<void>`. Exceções não atravessam fronteiras de módulo.
- Use `[[nodiscard]]` em funções cujo retorno não pode ser ignorado.
- Use `QStringLiteral` para literais `QString`.
- Os cabeçalhos usam `#pragma once`.
- Código que lê o sistema (pastas, registro do Windows, SO) recebe essas informações por parâmetro, como no `DetectionEnvironment`. Assim os testes podem simular qualquer SO com diretórios temporários.
- Em arquivos processados pelo moc (classes com `Q_OBJECT`, incluindo testes), evite raw string literals que contenham aspas e `//`: o moc deixa de reconhecer a classe e o link falha com `undefined reference to vtable`.
- Para validar documentos definidos pelas specs, use `saveforge::JsonReader` (`core`). Ele rejeita propriedades desconhecidas e informa o JSON pointer do erro.
- Escritas que precisam sobreviver a quedas seguem o padrão do vault: grave num arquivo temporário, faça `fsync` (`platform::syncFile`) e renomeie.
- Operações que alteram saves do usuário seguem o padrão da restauração: primeiro um plano sem efeitos colaterais, depois a execução com ponto de desfazer e rollback em caso de falha.
- Dependências de terceiros entram em `cmake/SaveForgeDependencies.cmake`: primeiro `find_package`, com fallback para download fixado e verificado por SHA-256. Cada dependência nova precisa de justificativa (ADR ou descrição no PR).
- Comentários explicam **por quê**, não o quê.

### QML
- Sempre use os tokens de `Theme` (cores, espaçamentos, raios, fontes). **Nunca** escreva cores ou tamanhos fixos nos componentes.
- Ícones vêm de `Icons`, em caminhos SVG numa grade 24x24.
- Use `pragma ComponentBehavior: Bound` em arquivos com delegates, e `required property` nos delegates.
- **Nomes de componentes não podem coincidir com tipos do Qt Quick Controls.** Um conflito real que já aconteceu: `SearchField`, que existe no Qt 6.10+.
- **Propriedades não podem começar com `on` seguido de maiúscula**, porque o QML as interpreta como handlers de sinal.
- Acessibilidade:
  - alvos de clique ≥ `Theme.minTouchTarget`;
  - foco visível;
  - `Accessible.name` em controles sem texto.
- Textos visíveis passam por `qsTr()`. A interface é escrita em pt-BR.
- **SaveForge.Ui é só apresentação.** Páginas leem e chamam o `AppController` (módulo SaveForge.Backend); componentes em `qml/components/` não conhecem o backend.
- Quantidades usam `Format.count(n, "jogo", "jogos")` no QML e `countText()` no C++ (número em pt-BR e plural correto). Não use textos como "jogo(s)".
- Operações demoradas nunca rodam na thread da interface: use o padrão `run()` do `AppController` (worker + resultado na thread da UI).

### Idiomas
- Código, identificadores, mensagens de commit e mensagens de log ficam em **inglês**.
- Textos da interface e documentação para usuários ficam em **pt-BR** (com tradução para inglês via Qt Linguist).

## Testes

- Toda mudança de comportamento vem com teste.
- Testes de unidade usam Qt Test e são registrados com `saveforge_add_test()` em `tests/CMakeLists.txt`.
- O teste `tst_ui_smoke` falha diante de **qualquer** aviso de QML. Componentes novos devem aparecer em alguma página ou ser exercitados pelo teste.
- Para revisar o visual, rode o teste com `SAVEFORGE_SCREENSHOT_DIR=<pasta>`. Ele salva uma captura de cada página.

## Commits

Usamos [Conventional Commits](https://www.conventionalcommits.org/), **em inglês**:

```
<type>(<scope>): <subject>

<body: o que mudou e por quê, quando não for óbvio>
```

- **`type`:** `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `build`, `ci`, `chore`.
- **`scope`:** o módulo afetado, como `core`, `ui`, `cli`, `vault` ou `detection`.
- **Assunto:** no imperativo e com até 72 caracteres, sem ponto final. Por exemplo: `feat(vault): add content-addressed blob store`.
- **Um commit, uma mudança lógica.** Cada commit deve compilar e passar nos testes.
- **Não adicione trailers de coautoria de ferramentas de IA** (por exemplo, `Co-Authored-By: Claude …`). O autor do commit é a pessoa responsável pela mudança.

## Pull requests

- Uma branch por mudança, com prefixo pelo tipo, por exemplo `feat/vault-blob-store` ou `fix/steam-library-path`.
- A descrição explica o problema, a solução e como testar. Mudanças de UI incluem capturas de tela.
- O CI (Windows, Linux e macOS) precisa passar.
- PRs grandes devem ser divididos sempre que possível.

## Documentação

- Uma API pública nova (cabeçalho em `include/`) recebe um comentário de cabeçalho explicando a responsabilidade do tipo ou da função.
- Mudanças de comportamento visível atualizam o `README.md`.
- Decisões arquiteturais viram ADR, em `saveforge-docs/adr/` quando afetam mais de um repositório, ou em `docs/adr/` quando são locais.
