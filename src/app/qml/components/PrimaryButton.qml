import QtQuick
import QtQuick.Controls.Basic

// Ação principal da região: fundo no acento Forja.
Button {
    id: control

    implicitHeight: Theme.minTouchTarget
    leftPadding: Theme.space5
    rightPadding: Theme.space5
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    font.weight: Font.Bold

    background: Rectangle {
        radius: Theme.radiusControl
        color: !control.enabled ? Theme.borderStrong
             : control.down ? Theme.accent
             : control.hovered ? Theme.accentHover : Theme.accent
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.text
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textOnAccent : Theme.textSubtle
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
