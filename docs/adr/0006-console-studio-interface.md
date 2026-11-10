# ADR 0006 (desktop) — Interface Console × Studio e barra de título integrada

**Status:** Aceita · **Data:** 2026-11-10 · **Substitui:** a direção Forja na interface do Desktop.

## Contexto
A interface Forja usava a moldura padrão do sistema, um fundo marrom-escuro fixo e o laranja como única cor. O mantenedor pediu três coisas:
- uma janela mais integrada ao Windows e ao macOS;
- um fundo mais neutro;
- mais personalização de cor, já que o público é de jogadores.

Das seis propostas comparadas, a escolhida foi a mistura das propostas Console e Studio (`savevault-docs/design/CONSOLE-STUDIO.md`).

## Decisão
- **Barra de título integrada:**
  - No Windows e no macOS, a janela usa `Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint` (Qt 6.9+).
  - A barra de título do app é **unificada**: ocupa a própria faixa do sistema (o "Command Center" do VS Code segue a mesma ideia), com o logo e a busca (`Ctrl+K`) ao lado dos botões nativos. Os botões continuam do sistema: minimizar, maximizar e fechar no Windows, os semáforos no macOS. Também ficam como estão as bordas, a sombra, o Snap e o redimensionamento.
  - **No Windows:** a janela não usa `Qt::WindowTitleHint` (fica só `CustomizeWindowHint` mais os botões). Assim o Qt não desenha o próprio ícone e título, que saíam com renderização ruim, e os cliques nessa faixa chegam ao app. O `ApplicationWindow` usa padding zero, para o conteúdo não começar abaixo da área segura.
  - **No macOS:** o título da janela fica vazio, porque o sistema o desenharia sobre a busca. O Dock e a barra de menus continuam mostrando o nome do app.
  - As áreas vazias da barra chamam `startSystemMove()`, e o duplo clique maximiza.
  - No Linux, a moldura do sistema continua, porque cada desktop decora as janelas de um jeito.
- **Tema:**
  - `Theme.qml` passa a ter variantes clara e escura.
  - O modo pode ser `system`, `dark` ou `light`, e o acento é qualquer `#RRGGBB`.
  - Os dois ficam em `settings.ini` (`appearance/theme`, `appearance/accent`) e são expostos por `AppController.themeMode` e `AppController.accentColor`.
  - As cores derivadas (tom de seleção, texto sobre o acento, acento como texto) são calculadas a partir do acento. Assim qualquer cor continua legível.
- **Biblioteca:** a lista densa e o jogo selecionado ficam lado a lado. A página de detalhes deixou de ser uma página separada.
- **Atalhos de teclado:**

  | Atalho | Ação |
  |---|---|
  | `Ctrl+K` | buscar |
  | `Ctrl+B` | backup do jogo selecionado |
  | `Ctrl+R` | restaurar o backup mais recente |
  | `Ctrl+T` | trocar de save |
  | setas | navegar pela lista |

  A barra inferior mostra os atalhos da página atual.
- **Fontes:** Outfit e JetBrains Mono (SIL OFL) entram no lugar da Space Grotesk. A Inter continua.

## Consequências
- (+) A janela parece nativa nos dois sistemas sem reimplementar os botões da barra de título nem o hit-test da moldura.
- (+) O jogador escolhe o tema e o acento, e a troca vale na hora para toda a interface.
- (−) A integração depende do Qt 6.9 ou mais novo. Num Qt anterior, a flag é ignorada e a moldura padrão aparece.
- (−) Ainda não há suporte a controle (gamepad). Os atalhos aparecem como teclas até essa parte existir.

## Alternativas
- **`Qt::FramelessWindowHint` com botões próprios:** controle total, mas no Windows perde o menu de Snap e precisa de redimensionamento manual pelas bordas.
- **Manter a moldura padrão:** era o que deixava o app com cara de "cru", que foi a reclamação original.
