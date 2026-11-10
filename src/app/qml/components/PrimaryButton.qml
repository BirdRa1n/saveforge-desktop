import QtQuick
import QtQuick.Controls.Basic

// Main action of a region, filled with the accent colour. `shortcutHint`
// (e.g. "Ctrl B") shows the keyboard shortcut that triggers it.
Button {
    id: control

    property string shortcutHint

    implicitHeight: Theme.minTouchTarget
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: Theme.radiusControl
        color: !control.enabled ? Theme.keycap
             : control.hovered || control.down ? Theme.accentHover : Theme.accent
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.text
    }

    contentItem: Row {
        spacing: Theme.space2

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.enabled ? Theme.textOnAccent : Theme.textSubtle
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.shortcutHint.length > 0
            width: hint.implicitWidth + 10
            height: 18
            radius: 5
            color: Qt.rgba(0, 0, 0, 0.16)
            Text {
                id: hint
                anchors.centerIn: parent
                text: control.shortcutHint
                color: control.enabled ? Theme.textOnAccent : Theme.textSubtle
                opacity: 0.85
                font.family: Theme.fontMono
                font.pixelSize: 10
            }
        }
    }
}
