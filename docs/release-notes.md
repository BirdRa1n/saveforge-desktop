> **Versão de testes (alpha).** Use-a com cópias dos seus saves ou junto de outro backup.

### Como instalar

| Sistema | Arquivo | Como usar |
|---|---|---|
| Windows 10/11 (x64) | `SaveForge-…-windows-x64-setup.exe` | **Recomendado.** Abra e siga o assistente. Não pede administrador, cria os atalhos no menu Iniciar e, se você quiser, na área de trabalho. Para remover, use "Adicionar ou remover programas". |
| Windows 10/11 (x64), portátil | `SaveForge-…-windows-x64.zip` | Extraia e abra `bin/SaveForge.exe`. A linha de comando fica em `bin/saveforge-cli.exe`. |
| Linux (x64) | `SaveForge-…-linux-x64.tar.gz` | Extraia e rode `bin/SaveForge`. Requer as bibliotecas gráficas do sistema (OpenGL, `libxkbcommon`, `libxcb-cursor0`). |
| macOS (Apple Silicon) | `SaveForge-…-macos-arm64.dmg` | Abra o disco e arraste o SaveForge para Aplicativos. |

Cada arquivo vem com um `.sha256` para conferir o download.

### Limitações desta versão

- **Sem assinatura de código.** O Windows SmartScreen pode avisar sobre um app desconhecido: clique em "Mais informações" e depois em "Executar assim mesmo". No macOS, abra o app pela primeira vez com clique direito e "Abrir".
- **Perfis assinados com chaves de teste.** Os perfis embutidos e os baixados vêm do Registry de desenvolvimento, assinado com chaves de teste públicas. As chaves de produção chegam antes da versão beta.
- Os perfis cobrem os locais de save do [manifesto do Ludusavi](https://github.com/mtkennerly/ludusavi-manifest) (CC BY-NC-SA 3.0, dados do PCGamingWiki).
