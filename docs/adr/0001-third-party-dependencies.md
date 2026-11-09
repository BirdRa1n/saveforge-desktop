# ADR 0001 (desktop) — Dependências de terceiros: sistema primeiro, FetchContent com hash como fallback

**Status:** Aceita · **Data:** 2026-11-09 · **Ajusta:** ADR 0002 do ecossistema (`saveforge-docs/adr/0002-cpp20-qt6-toolchain.md`), que previa vcpkg.

## Contexto
O vault precisa do zstd, a primeira dependência além do Qt. A ADR 0002 do ecossistema propunha o vcpkg em modo manifest. Na prática:
- o ambiente de desenvolvimento atual usa o **MinGW** do instalador do Qt, e o vcpkg com MinGW depende de triplets da comunidade e de um clone completo do vcpkg;
- o CI e os contribuidores precisariam instalar e configurar o vcpkg só para isso;
- distribuições Linux e o Flatpak preferem usar as bibliotecas do sistema.

## Decisão
Cada dependência é resolvida em `cmake/SaveForgeDependencies.cmake` com `FetchContent_Declare(... FIND_PACKAGE_ARGS ...)`:
1. **Sistema primeiro:** `find_package` encontra a biblioteca instalada no sistema, no SDK do Flatpak, ou num toolchain do vcpkg, se o desenvolvedor usar um.
2. **Fallback reprodutível:** se não encontrar, o CMake baixa um release **fixo** e confere o **SHA-256** (copiado do checksum publicado pelo próprio projeto) antes de compilar.
3. Os módulos linkam um alvo `saveforge::<dep>`, que esconde a origem da biblioteca.

## Consequências
- (+) Funciona igual com MinGW, MSVC, GCC e Clang, sem nenhuma configuração extra.
- (+) Um download adulterado falha no hash, e o build para.
- (−) O primeiro configure precisa de internet quando a dependência não está no sistema. Builds offline exigem a dependência instalada ou um `FETCHCONTENT_SOURCE_DIR_<DEP>` apontando para o código-fonte.
- (−) Atualizar uma dependência significa trocar a URL e o hash à mão, em um commit próprio.

## Alternativas
- **vcpkg (manifest):** continua possível como toolchain opcional, porque o `find_package` o encontra. Não será obrigatório.
- **Copiar o código-fonte para o repositório (vendoring):** histórico maior e atualizações manuais.
- **Conan:** mesma objeção do vcpkg (setup extra).
