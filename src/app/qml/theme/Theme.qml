pragma Singleton
import QtQuick

// Tokens da direção visual "Forja" (ver saveforge-docs/design/FORJA.md).
QtObject {
    // Superfícies
    readonly property color bg: "#16130F"
    readonly property color surface: "#1E1A15"
    readonly property color card: "#1F1A14"
    readonly property color surfaceRaised: "#251F18"
    readonly property color control: "#221D17"
    readonly property color selected: "#2A231B"
    readonly property color border: "#2E2820"
    readonly property color borderStrong: "#3A3127"

    // Texto
    readonly property color text: "#F5EFE6"
    readonly property color textMuted: "#BFB3A3"
    readonly property color textSubtle: "#A89C8C"

    // Acento
    readonly property color accent: "#F97316"
    readonly property color accentHover: "#FB8A3C"
    // (não usar o prefixo "on": QML o interpreta como handler de sinal)
    readonly property color textOnAccent: "#1A1208"
    readonly property color accentSoft: "#FDBA74"

    // Status (sempre acompanhados de rótulo em texto)
    readonly property color successBg: "#1F3324"
    readonly property color successFg: "#86EFAC"
    readonly property color warningBg: "#3A2A12"
    readonly property color warningFg: "#FCD34D"
    readonly property color neutralBg: "#2E2820"
    readonly property color neutralFg: "#D6CBBD"

    // Tipografia (fontes OFL a serem empacotadas; até lá, cai na fonte do sistema)
    readonly property string fontDisplay: "Space Grotesk"
    readonly property string fontBody: "Inter"
    readonly property int textPageTitle: 30
    readonly property int textSectionTitle: 20
    readonly property int textBody: 14
    readonly property int textMeta: 12

    // Espaçamento
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space8: 32

    // Raios
    readonly property int radiusBadge: 6
    readonly property int radiusControl: 10
    readonly property int radiusCard: 14

    // Acessibilidade
    readonly property int minTouchTarget: 44
}
