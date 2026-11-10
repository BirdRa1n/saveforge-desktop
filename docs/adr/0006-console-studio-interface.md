# ADR 0006 (desktop) — Interface Console × Studio e barra de título integrada

**Status:** Aceita · **Data:** 2026-11-10 · **Substitui:** a direção Forja na interface do Desktop.

## Contexto
A interface Forja usava a moldura padrão do sistema, um fundo marrom-escuro fixo e o laranja como única cor. O mantenedor pediu três coisas:
- uma janela mais integrada ao Windows e ao macOS;
- um fundo mais neutro;
- mais personalização de cor, já que o público é de jogadores.

Das seis propostas comparadas, a escolhida foi a mistura das propostas Console e Studio (`saveforge-docs/design/CONSOLE-STUDIO.md`).

## Decisão
- **Barra de título integrada:**
  - No Windows e no macOS, a janela usa `Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint` (Qt 6.9+).
  - O conteúdo começa abaixo da área segura (`SafeArea.margins`), e o sistema continua desenhando os botões nativos: minimizar, maximizar e fechar no Windows, os semáforos no macOS. Também ficam como estão as bordas, a sombra, o Snap e o redimensionamento.
  - A barra de busca fica logo abaixo dessa faixa. As áreas vazias chamam `startSystemMove()`, e o duplo clique maximiza.
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
