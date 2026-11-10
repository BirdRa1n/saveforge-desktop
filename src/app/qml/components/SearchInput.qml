import QtQuick
import QtQuick.Controls.Basic

// Search field with a magnifier and, optionally, the keyboard shortcut that
// focuses it (`shortcutHint`, e.g. "Ctrl K").
TextField {
    id: control

    property string shortcutHint

    implicitHeight: Theme.minTouchTarget
    leftPadding: 36
    rightPadding: hint.visible ? hint.width + Theme.space4 : Theme.space3
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    font.family: Theme.fontBody
    font.pixelSize: 13
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.control
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border

        Icon {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 15
            height: 15
            path: Icons.search
            color: Theme.textMuted
        }

        KeyCap {
            id: hint
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            visible: control.shortcutHint.length > 0 && !control.activeFocus && control.text.length === 0
            text: control.shortcutHint
        }
    }
}
