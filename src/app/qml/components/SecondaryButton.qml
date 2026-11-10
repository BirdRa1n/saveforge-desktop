import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    // Optional icon (an Icons path) shown before the text.
    property string iconPath

    implicitHeight: Theme.minTouchTarget
    leftPadding: Theme.space4 + 2
    rightPadding: Theme.space4 + 2
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.down ? Theme.selected
             : control.hovered && control.enabled ? Theme.surfaceRaised : Theme.control
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.borderStrong
    }

    contentItem: Row {
        spacing: Theme.space2

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconPath.length > 0
            width: 18
            height: 18
            path: control.iconPath
            color: control.enabled ? Theme.text : Theme.textSubtle
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.enabled ? Theme.text : Theme.textSubtle
        }
    }
}
