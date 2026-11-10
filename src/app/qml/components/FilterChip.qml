import QtQuick
import QtQuick.Controls.Basic

// Filter chip: the active one is filled with the accent, the others are tinted.
AbstractButton {
    id: control

    checkable: true
    hoverEnabled: true
    implicitHeight: 30
    implicitWidth: label.implicitWidth + 2 * Theme.space4 - 2
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: control.checked ? Theme.accent : control.hovered ? Theme.surfaceRaised : Theme.keycap
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.text
    }

    contentItem: Text {
        id: label
        text: control.text
        color: control.checked ? Theme.textOnAccent : Theme.chipText
        font.family: Theme.fontBody
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
