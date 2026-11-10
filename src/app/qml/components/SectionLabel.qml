import QtQuick

// Small uppercase heading of a section inside a panel ("HISTÓRICO DE BACKUPS").
Text {
    color: Theme.textSubtle
    font.family: Theme.fontBody
    font.pixelSize: Theme.textMeta
    font.weight: Font.DemiBold
    font.letterSpacing: 0.6
    font.capitalization: Font.AllUppercase
    Accessible.role: Accessible.Heading
}
