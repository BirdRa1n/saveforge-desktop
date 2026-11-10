import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Check box with a label, for choosing items in a list. The whole row is clickable.
AbstractButton {
    id: control

    // Optional second line under the label.
    property string detail

    checkable: true
    implicitHeight: Math.max(Theme.minTouchTarget, row.implicitHeight)
    implicitWidth: row.implicitWidth
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.hovered ? Theme.surfaceRaised : "transparent"
    }

    contentItem: RowLayout {
        id: row
        spacing: Theme.space3

        Rectangle {
            Layout.leftMargin: Theme.space2
            implicitWidth: 20
            implicitHeight: 20
            radius: 5
            color: control.checked ? Theme.accent : Theme.control
            border.width: control.visualFocus ? 2 : 1
            border.color: control.visualFocus ? Theme.accentSoft : (control.checked ? Theme.accent : Theme.borderStrong)

            Icon {
                anchors.centerIn: parent
                width: 14
                height: 14
                visible: control.checked
                path: Icons.check
                color: Theme.textOnAccent
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: control.text
                color: Theme.text
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: control.detail.length > 0
                text: control.detail
                color: Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: Theme.textMeta
                elide: Text.ElideRight
            }
        }
    }
}
