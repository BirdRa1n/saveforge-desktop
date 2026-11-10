# ADR 0005 (desktop) — miniz para os arquivos `.gamesave`

**Status:** Aceita · **Data:** 2026-11-10 · **Implementa:** tarefas D2.1 e D2.5 do roadmap (`saveforge-specs/gamesave`).

## Contexto
A spec define o `.gamesave` como um arquivo ZIP, com entradas armazenadas ou comprimidas com deflate e com ZIP64 permitido. O leitor precisa inspecionar cada entrada antes de extrair: nome, tipo, criptografia e tamanhos declarados. O Qt não tem uma API pública de ZIP; `QZipReader` e `QZipWriter` são privados e mudam entre versões.

## Decisão
Usar o **miniz 3.1.2**, a versão amalgamada (`miniz.c` + `miniz.h`, licença MIT):
- O download é fixado e verificado por SHA-256. O hash é o mesmo que o GitHub publica para o arquivo da release. A biblioteca é compilada como `saveforge::miniz`, seguindo a ADR 0001.
- O miniz só é usado dentro do novo módulo `archive`, que lê e grava o arquivo pelo `QFile` (callbacks de leitura e escrita). Assim os caminhos com acentos funcionam igual em todos os sistemas.
- Na leitura, o módulo aplica cada regra da seção 3 da spec antes de importar qualquer coisa:
  - `mimetype` como primeira entrada e sem compressão;
  - nomes seguros e únicos;
  - nenhuma entrada criptografada ou especial;
  - limites de tamanho e de taxa de compressão;
  - hash do manifest;
  - hash de cada blob.
- Na escrita, uma entrada só é comprimida quando isso ajuda e quando não ultrapassa a taxa de compressão que os leitores aceitam. O arquivo é gravado ao lado do destino, reaberto, verificado e só então renomeado.

## Consequências
- (+) Dois arquivos C, sem outras dependências, que compilam com MinGW, MSVC, GCC e Clang.
- (+) O leitor tem controle das entradas antes de extrair, o que a defesa contra arquivos maliciosos exige.
- (−) A versão amalgamada não traz um projeto CMake. Por isso o `FetchContent` só baixa os arquivos, e o alvo é definido aqui.

## Alternativas
- **QZipReader/QZipWriter (privados do Qt):** sem dependência nova, mas presos aos cabeçalhos privados da versão exata do Qt.
- **libzip ou minizip-ng:** mais completas, mas dependem de zlib e de mais configuração em cada sistema.
- **libarchive:** grande demais para um formato só.
