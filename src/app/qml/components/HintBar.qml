pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import SaveForge.Backend

// Bottom bar: the keyboard shortcuts of the current page and the state of
// automatic backups and of the vault.
Item {
    id: root

    // [{ key: "Ctrl B", label: "Fazer backup" }, ...]
    property var hints: []
    property string statusText

    implicitHeight: 34

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space5
        anchors.rightMargin: Theme.space5
        spacing: Theme.space5

        Repeater {
            model: root.hints
            delegate: RowLayout {
                id: hint
                required property var modelData
                spacing: 6
                KeyCap { text: hint.modelData.key }
                Text {
                    text: hint.modelData.label
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textMeta
                }
            }
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            readonly property bool active: AppController.backupOnGameExit || AppController.backupIntervalHours > 0
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 4
            color: active ? Theme.successFg : Theme.textSubtle
        }
        Text {
            Layout.maximumWidth: root.width / 2
            text: ((AppController.backupOnGameExit || AppController.backupIntervalHours > 0)
                   ? qsTr("Backup automático ativo") : qsTr("Backup automático desligado"))
                  + (root.statusText.length > 0 ? "  ·  " + root.statusText : "")
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textMeta
            elide: Text.ElideRight
        }
    }
}
