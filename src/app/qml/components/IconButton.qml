import QtQuick
import QtQuick.Controls.Basic

// Square button that shows only an icon. `text` is its accessible name and
// tooltip. With `checkable`, a checked button is drawn in the accent colour.
AbstractButton {
    id: control

    property string iconPath

    implicitWidth: Theme.minTouchTarget
    implicitHeight: Theme.minTouchTarget
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    ToolTip.visible: hovered && text.length > 0
    ToolTip.delay: 500
    ToolTip.text: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.down ? Theme.selected
             : control.hovered && control.enabled ? Theme.surfaceRaised : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: 20
            height: 20
            path: control.iconPath
            color: !control.enabled ? Theme.textSubtle : control.checked ? Theme.accent : Theme.textMuted
        }
    }
}
