import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    implicitHeight: Theme.minTouchTarget
    leftPadding: 40
    rightPadding: Theme.space3
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.control
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.borderStrong

        Icon {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            path: Icons.search
            color: Theme.textMuted
        }
    }
}
