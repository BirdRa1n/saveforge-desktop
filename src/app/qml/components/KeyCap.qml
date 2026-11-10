import QtQuick

// A keyboard key, as in "Ctrl K": shows which shortcut triggers an action.
// Decorative; the action's own accessible name carries the meaning.
Rectangle {
    id: root

    property alias text: label.text

    implicitWidth: Math.max(20, label.implicitWidth + 10)
    implicitHeight: 20
    radius: 5
    color: Theme.keycap
    Accessible.ignored: true

    Text {
        id: label
        anchors.centerIn: parent
        color: Theme.textMuted
        font.family: Theme.fontMono
        font.pixelSize: 10
        font.weight: Font.Medium
    }
}
