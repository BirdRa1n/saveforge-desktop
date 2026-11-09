import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// On/off setting: a label followed by a switch. The whole row is clickable.
AbstractButton {
    id: control

    checkable: true
    implicitHeight: Math.max(Theme.minTouchTarget, row.implicitHeight)
    implicitWidth: row.implicitWidth
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    background: Item {}

    contentItem: RowLayout {
        id: row
        spacing: Theme.space3

        Rectangle {
            id: track
            implicitWidth: 40
            implicitHeight: 22
            radius: height / 2
            color: control.checked ? Theme.accent : Theme.control
            border.width: control.visualFocus ? 2 : 1
            border.color: control.visualFocus ? Theme.accentSoft : (control.checked ? Theme.accent : Theme.borderStrong)

            Rectangle {
                width: track.height - 6
                height: width
                radius: width / 2
                y: 3
                x: control.checked ? track.width - width - 3 : 3
                color: control.checked ? Theme.textOnAccent : Theme.textMuted
                Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            }
        }

        Text {
            Layout.fillWidth: true
            text: control.text
            color: Theme.text
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            wrapMode: Text.Wrap
        }
    }
}
