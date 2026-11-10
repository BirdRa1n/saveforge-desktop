pragma Singleton
import QtQuick
import SaveForge.Backend

// Design tokens of the "Console × Studio" direction (see
// saveforge-docs/design/CONSOLE-STUDIO.md): neutral blue-grey surfaces in dark
// and light variants, and one accent colour the user picks in Configurações.
// Components read only these tokens, so the theme switches everywhere at once.
QtObject {
    id: theme

    // --- Appearance -------------------------------------------------------
    // "system" follows the OS colour scheme; "dark" and "light" force one.
    readonly property string mode: AppController.themeMode
    readonly property bool dark: mode === "dark"
                                 || (mode === "system" && Application.styleHints.colorScheme !== Qt.ColorScheme.Light)

    // Accents offered in Configurações; any #RRGGBB works.
    readonly property var accentPresets: ["#22D3EE", "#3B82F6", "#8B5CF6", "#22C55E", "#F97316", "#EC4899"]

    // --- Surfaces ----------------------------------------------------------
    // window: the frame (title bar, rail, hint bar); bg: the content panels.
    readonly property color window: dark ? "#0F1218" : "#E9EDF2"
    readonly property color bg: dark ? "#151922" : "#FFFFFF"
    readonly property color surface: dark ? "#1B2029" : "#F6F8FA"
    readonly property color card: dark ? "#1B2029" : "#F6F8FA"
    readonly property color surfaceRaised: dark ? "#222834" : "#EEF1F5"
    readonly property color control: dark ? "#1B2029" : "#FFFFFF"
    readonly property color selected: accentTint
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(18 / 255, 22 / 255, 32 / 255, 0.10)
    readonly property color borderSoft: dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(18 / 255, 22 / 255, 32 / 255, 0.06)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(18 / 255, 22 / 255, 32 / 255, 0.18)
    readonly property color keycap: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(18 / 255, 22 / 255, 32 / 255, 0.08)
    readonly property color scrim: Qt.rgba(0, 0, 0, dark ? 0.55 : 0.35)

    // --- Text --------------------------------------------------------------
    readonly property color text: dark ? "#ECEFF4" : "#121620"
    readonly property color textMuted: dark ? "#A3ACBA" : "#4F5868"
    readonly property color textSubtle: dark ? "#8D96A5" : "#5F6878"
    readonly property color chipText: textMuted

    // --- Accent ------------------------------------------------------------
    readonly property color accent: AppController.accentColor
    readonly property color accentHover: dark ? Qt.lighter(accent, 1.12) : Qt.darker(accent, 1.08)
    readonly property color accentTint: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.14 : 0.12)
    readonly property color accentSoft: Qt.lighter(accent, 1.3)
    // Text on the accent: dark ink on light accents, white on dark ones.
    // (Never name a property with the "on" prefix: QML reads it as a handler.)
    readonly property color textOnAccent: luminance(accent) > 0.45 ? "#0B0E14" : "#FFFFFF"
    // Accent used as text or icon on the panels: darker in light mode for contrast.
    readonly property color accentText: dark ? accent : Qt.darker(accent, luminance(accent) > 0.45 ? 1.6 : 1.1)

    function luminance(c) {
        const lin = v => v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
        return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b)
    }

    // --- Status (always paired with a text label) ---------------------------
    readonly property color successFg: dark ? "#4ADE80" : "#15803D"
    readonly property color successBg: dark ? Qt.rgba(74 / 255, 222 / 255, 128 / 255, 0.14) : Qt.rgba(21 / 255, 128 / 255, 61 / 255, 0.10)
    readonly property color warningFg: dark ? "#FBBF24" : "#B45309"
    readonly property color warningBg: dark ? Qt.rgba(251 / 255, 191 / 255, 36 / 255, 0.14) : Qt.rgba(180 / 255, 83 / 255, 9 / 255, 0.10)
    readonly property color dangerFg: dark ? "#F87171" : "#B91C1C"
    readonly property color neutralFg: dark ? "#C3CAD5" : "#3B4250"
    readonly property color neutralBg: keycap

    // Controller face buttons, shown next to the actions they trigger.
    readonly property color padA: "#22C55E"
    readonly property color padX: "#3B82F6"
    readonly property color padY: "#EAB308"

    // --- Typography ----------------------------------------------------------
    // Inter, Outfit and JetBrains Mono (SIL OFL 1.1) ship with the app in
    // src/app/fonts. Each FontLoader registers one weight of a family.
    readonly property list<FontLoader> fonts: [
        FontLoader { source: "../../fonts/inter/Inter-Regular.ttf" },
        FontLoader { source: "../../fonts/inter/Inter-Medium.ttf" },
        FontLoader { source: "../../fonts/inter/Inter-SemiBold.ttf" },
        FontLoader { source: "../../fonts/inter/Inter-Bold.ttf" },
        FontLoader { source: "../../fonts/outfit/Outfit-Regular.ttf" },
        FontLoader { source: "../../fonts/outfit/Outfit-Medium.ttf" },
        FontLoader { source: "../../fonts/outfit/Outfit-SemiBold.ttf" },
        FontLoader { source: "../../fonts/outfit/Outfit-Bold.ttf" },
        FontLoader { source: "../../fonts/jetbrains-mono/JetBrainsMono-Regular.ttf" },
        FontLoader { source: "../../fonts/jetbrains-mono/JetBrainsMono-Medium.ttf" }
    ]
    readonly property string fontDisplay: "Outfit"
    readonly property string fontBody: "Inter"
    readonly property string fontMono: "JetBrains Mono"
    readonly property int textPageTitle: 26
    readonly property int textSectionTitle: 18
    readonly property int textBody: 14
    readonly property int textMeta: 12

    // --- Spacing -------------------------------------------------------------
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space8: 32

    // --- Radii ---------------------------------------------------------------
    readonly property int radiusBadge: 6
    readonly property int radiusControl: 8
    readonly property int radiusCard: 12
    readonly property int radiusPanel: 14

    // --- Accessibility -------------------------------------------------------
    readonly property int minTouchTarget: 40
}
