# Ícone do app

A fonte do ícone é [`src/app/icons/savevault.svg`](../../src/app/icons/savevault.svg): a porta de cofre de `Icons.vault` sobre um ladrilho cinza-azulado, no acento ciano padrão da direção Console × Studio. Os arquivos de cada sistema são gerados a partir dela e ficam versionados ao lado:

| Arquivo | Uso |
|---|---|
| `savevault.ico` | Executável e instalador no Windows (16 a 256 px) |
| `savevault.icns` | Bundle do macOS (16 a 1024 px, com variantes Retina) |
| `savevault.png` | Ícone da janela e entrada `.desktop` no Linux (256 px) |

Depois de mudar o SVG, gere os arquivos de novo e faça commit deles junto. São necessários o Python 3, o CMake, o Ninja, um compilador e o Qt com o módulo Svg:

```bash
python tools/icons/generate.py --qt C:/Qt/6.12.0/mingw_64
```

O script compila o `render.cpp`, um renderizador mínimo com o `QSvgRenderer`, desenha o SVG em cada tamanho e empacota os PNGs nos formatos `.ico` e `.icns`.
