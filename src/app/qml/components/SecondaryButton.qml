import QtQuick
import QtQuick.Controls.Basic

// Secondary action: outlined surface. Optional icon (an Icons path) before the
// text and `shortcutHint` (e.g. "Ctrl R") after it.
Button {
    id: control

    property string iconPath
    property string shortcutHint

    implicitHeight: Theme.minTouchTarget
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    font.weight: Font.Medium

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.down ? Theme.accentTint
             : control.hovered && control.enabled ? Theme.surfaceRaised : Theme.control
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.border
    }

    contentItem: Row {
        spacing: Theme.space2

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconPath.length > 0
            width: 17
            height: 17
            path: control.iconPath
            color: control.enabled ? Theme.text : Theme.textSubtle
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.enabled ? Theme.text : Theme.textSubtle
        }
        KeyCap {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.shortcutHint.length > 0
            text: control.shortcutHint
        }
    }
}
