import QtQuick
import QtQuick.Controls.Basic

// Chip de filtro: ativo = preenchido; inativo = só contorno.
AbstractButton {
    id: control

    checkable: true
    implicitHeight: 36
    implicitWidth: label.implicitWidth + 2 * Theme.space4 - 2
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: control.checked ? Theme.text : "transparent"
        border.width: control.checked ? (control.visualFocus ? 2 : 0) : (control.visualFocus ? 2 : 1)
        border.color: control.visualFocus ? Theme.accent : Theme.borderStrong
    }

    contentItem: Text {
        id: label
        text: control.text
        color: control.checked ? Theme.bg : "#CDBFAE"
        font.family: Theme.fontBody
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
