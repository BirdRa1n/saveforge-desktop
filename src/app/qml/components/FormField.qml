import QtQuick
import QtQuick.Controls.Basic

// Single-line text input for forms (SearchInput is the variant with a search icon).
TextField {
    id: control

    implicitHeight: Theme.minTouchTarget
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.control
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.borderStrong
    }
}
