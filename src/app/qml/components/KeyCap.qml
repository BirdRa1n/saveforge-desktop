import QtQuick

// A keyboard key, as in "Ctrl K": shows which shortcut triggers an action.
// Decorative; the action's own accessible name carries the meaning.
Rectangle {
    id: root

    property alias text: label.text
    // Smaller key, for controls in the title bar.
    property bool compact: false

    implicitWidth: Math.max(compact ? 16 : 20, label.implicitWidth + (compact ? 8 : 10))
    implicitHeight: compact ? 16 : 20
    radius: compact ? 4 : 5
    color: Theme.keycap
    Accessible.ignored: true

    Text {
        id: label
        anchors.centerIn: parent
        color: Theme.textMuted
        font.family: Theme.fontMono
        font.pixelSize: root.compact ? 9 : 10
        font.weight: Font.Medium
    }
}
