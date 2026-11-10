> **Versão de testes (alpha).** Use-a com cópias dos seus saves ou junto de outro backup.

### Novidades da 0.3

- **Novo nome: SaveVault.** Até a 0.2, o app se chamava SaveForge. Ao abrir a versão nova pela primeira vez, as configurações e os backups da pasta antiga passam para a nova automaticamente. Os arquivos `.gamesave` exportados antes continuam sendo importados normalmente. No Windows, o instalador atualiza a instalação anterior.
- **Interface nova:**
  - **Biblioteca:** a lista de jogos fica ao lado do jogo selecionado, que mostra a arte, os botões principais, os saves com nome e o histórico.
  - **Janela:** no Windows e no macOS, o app ocupa a área da barra de título e mantém os botões nativos da janela.
  - **Tema e cor:** escolha entre os temas claro, escuro ou do sistema e uma das 6 cores de destaque, em Configurações › Aparência.
  - **Atalhos:** `Ctrl+K` busca, `Ctrl+B` faz backup, `Ctrl+R` restaura, `Ctrl+T` troca de save e as setas navegam pela lista.
- **Ícone novo:** uma porta de cofre.

### Correções da alpha.2

- **Barra de título:** no Windows e no macOS, o logo e a busca agora ficam na mesma faixa dos botões da janela. O título borrado que aparecia no Windows não existe mais.
- **Capas:** as capas da biblioteca ficaram nítidas; antes elas eram ampliadas e saíam borradas.
- **Lista:** o status de backup fica sempre alinhado, mesmo com nomes de jogos longos.

### Como instalar

| Sistema | Arquivo | Como usar |
|---|---|---|
| Windows 10/11 (x64) | `SaveVault-…-windows-x64-setup.exe` | **Recomendado.** Abra e siga o assistente. Não pede administrador, cria os atalhos no menu Iniciar e, se você quiser, na área de trabalho. Para remover, use "Adicionar ou remover programas". |
| Windows 10/11 (x64), portátil | `SaveVault-…-windows-x64.zip` | Extraia e abra `bin/SaveVault.exe`. A linha de comando fica em `bin/savevault-cli.exe`. |
| Linux (x64) | `SaveVault-…-linux-x64.tar.gz` | Extraia e rode `bin/SaveVault`. Requer as bibliotecas gráficas do sistema (OpenGL, `libxkbcommon`, `libxcb-cursor0`). |
| macOS (Apple Silicon) | `SaveVault-…-macos-arm64.dmg` | Abra o disco e arraste o SaveVault para Aplicativos. |

Cada arquivo vem com um `.sha256` para conferir o download.

### Limitações desta versão

- **Sem assinatura de código.** O Windows SmartScreen pode avisar sobre um app desconhecido: clique em "Mais informações" e depois em "Executar assim mesmo". No macOS, abra o app pela primeira vez com clique direito e "Abrir".
- **Perfis assinados com chaves de teste.** Os perfis embutidos e os baixados vêm do Registry de desenvolvimento, assinado com chaves de teste públicas. As chaves de produção chegam antes da versão beta.
- Os perfis cobrem os locais de save do [manifesto do Ludusavi](https://github.com/mtkennerly/ludusavi-manifest) (CC BY-NC-SA 3.0, dados do PCGamingWiki).
