import QtQuick
import QtQuick.Controls.Basic

// Icon button of the navigation rail. `checked` marks the current page; `text`
// is the tooltip and the accessible name.
AbstractButton {
    id: control

    property string iconPath

    checkable: true
    implicitWidth: 44
    implicitHeight: 44
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.PageTab
    Accessible.name: text

    ToolTip.visible: hovered
    ToolTip.delay: 400
    ToolTip.text: text

    background: Rectangle {
        radius: Theme.radiusCard
        color: control.checked ? Theme.accentTint
             : control.hovered ? Theme.surfaceRaised : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            width: 20
            height: 20
            path: control.iconPath
            color: control.checked ? Theme.accentText : Theme.textMuted
        }
    }
}
