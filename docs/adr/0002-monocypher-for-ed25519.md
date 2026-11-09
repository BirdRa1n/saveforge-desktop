# ADR 0002 (desktop) — Monocypher para verificar assinaturas Ed25519

**Status:** Aceita · **Data:** 2026-11-09 · **Ajusta:** ADR 0006 do ecossistema, que previa libsodium.

## Contexto
O Desktop precisa verificar as assinaturas Ed25519 do Registry (`saveforge-specs/registry`). A ADR 0006 do ecossistema citava a libsodium, mas:
- a libsodium não tem build CMake oficial: usa autotools ou projetos do Visual Studio. Isso complica o padrão de dependências da ADR 0001 (sistema primeiro, com fallback para `FetchContent` verificado por hash) no MinGW e no MSVC;
- o Desktop só precisa **verificar** assinaturas, e não da biblioteca inteira.

## Decisão
Usar o **Monocypher 4.0.3**: `monocypher.c` mais o opcional `monocypher-ed25519.c` (Ed25519 com SHA-512, compatível com o RFC 8032).
- O download é fixado e verificado por SHA-512 (o mesmo checksum publicado em monocypher.org), e a biblioteca é compilada como `saveforge::monocypher`.
- O uso fica encapsulado em `saveforge::registry::verifyEd25519()`, testado com os vetores de `saveforge-specs/registry/test-vectors/signing.json`.

## Consequências
- (+) Dois arquivos C, sem dependências, licença CC0 ou BSD-2 (à escolha), com auditoria pública (Cure53, 2020).
- (+) Compila igual com MinGW, MSVC, GCC e Clang.
- (−) A criptografia de ponta a ponta da nuvem (Fase 6) vai precisar de primitivas extras. O Monocypher já oferece XChaCha20-Poly1305 e Argon2, então a troca de biblioteca deve continuar desnecessária.

## Alternativas
- **libsodium:** padrão de mercado, mas com integração de build mais trabalhosa.
- **OpenSSL:** grande demais para o que precisamos, e a distribuição varia por SO.
- **Implementação própria:** fora de questão para criptografia.
