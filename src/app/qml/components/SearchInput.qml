import QtQuick
import QtQuick.Controls.Basic

// Search field with a magnifier and, optionally, the keyboard shortcut that
// focuses it (`shortcutHint`, e.g. "Ctrl K").
TextField {
    id: control

    property string shortcutHint
    // Smaller text, icon and key hint, for the title bar.
    property bool compact: false

    implicitHeight: Theme.minTouchTarget
    leftPadding: compact ? 30 : 36
    rightPadding: hint.visible ? hint.width + Theme.space4 : Theme.space3
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    font.family: Theme.fontBody
    font.pixelSize: compact ? 12 : 13
    Accessible.name: placeholderText

    background: Rectangle {
        radius: control.compact ? 6 : Theme.radiusControl
        color: Theme.control
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border

        Icon {
            x: control.compact ? 10 : 12
            anchors.verticalCenter: parent.verticalCenter
            width: control.compact ? 13 : 15
            height: control.compact ? 13 : 15
            path: Icons.search
            color: Theme.textMuted
        }

        KeyCap {
            id: hint
            anchors.right: parent.right
            anchors.rightMargin: control.compact ? 5 : Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            compact: control.compact
            visible: control.shortcutHint.length > 0 && !control.activeFocus && control.text.length === 0
            text: control.shortcutHint
        }
    }
}
