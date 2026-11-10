pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

// Drop-down list of choices in the Forja style. `model` is a list of
// {text, value}; choosing one emits triggered(value).
Popup {
    id: root

    property var model: []
    signal triggered(var value)

    padding: Theme.space1
    implicitWidth: Math.max(240, column.implicitWidth + 2 * padding)

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong
    }

    contentItem: Column {
        id: column

        Repeater {
            model: root.model
            delegate: ItemDelegate {
                id: entry
                required property var modelData
                width: Math.max(implicitWidth, root.availableWidth)
                implicitHeight: Theme.minTouchTarget
                leftPadding: Theme.space3
                rightPadding: Theme.space3
                text: modelData.text
                background: Rectangle {
                    radius: Theme.radiusControl - 2
                    color: entry.hovered || entry.visualFocus ? Theme.selected : "transparent"
                }
                contentItem: Text {
                    text: entry.text
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
                onClicked: {
                    root.close()
                    root.triggered(entry.modelData.value)
                }
            }
        }
    }
}
