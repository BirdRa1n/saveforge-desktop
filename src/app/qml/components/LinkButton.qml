import QtQuick
import QtQuick.Controls.Basic

// Low-emphasis action drawn as accent text, for actions repeated in lists.
AbstractButton {
    id: control

    implicitWidth: label.implicitWidth + 2 * Theme.space2
    implicitHeight: 32
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.hovered && control.enabled ? Theme.accentTint : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: Text {
        id: label
        text: control.text
        color: control.enabled ? Theme.accentText : Theme.textSubtle
        font.family: Theme.fontBody
        font.pixelSize: 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
