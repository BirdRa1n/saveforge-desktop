import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Item da barra lateral. `checked` marca a página atual.
AbstractButton {
    id: control

    property string iconPath

    checkable: true
    implicitHeight: Theme.minTouchTarget
    Layout.fillWidth: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.PageTab
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.checked ? Theme.selected
             : control.hovered ? Theme.surfaceRaised : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: RowLayout {
        spacing: Theme.space3

        Item { Layout.preferredWidth: 0 }

        Icon {
            path: control.iconPath
            color: control.checked ? Theme.text : Theme.textMuted
        }

        Text {
            Layout.fillWidth: true
            text: control.text
            color: control.checked ? Theme.text : Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            font.weight: control.checked ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }
    }
}
