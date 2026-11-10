pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// What happened in this session: scans, backups, restores, imports.
Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space8
        anchors.topMargin: 28
        spacing: 22

        Text {
            text: qsTr("Atividade")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textPageTitle
            font.weight: Font.Bold
            font.letterSpacing: -0.6
            Accessible.role: Accessible.Heading
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            spacing: Theme.space2
            model: AppController.activity
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                id: entry
                required property var modelData
                width: list.width
                spacing: Theme.space3

                Text {
                    Layout.preferredWidth: 48
                    text: entry.modelData.time
                    color: Theme.textSubtle
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textMeta
                }
                Rectangle {
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: entry.modelData.kind === "error" ? Theme.dangerFg
                         : entry.modelData.kind === "warning" ? Theme.warningFg
                         : entry.modelData.kind === "success" ? Theme.successFg : Theme.textSubtle
                }
                Text {
                    Layout.fillWidth: true
                    text: entry.modelData.text
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    wrapMode: Text.Wrap
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: list.count === 0
            iconPath: Icons.activity
            title: qsTr("Nenhuma atividade ainda")
            description: qsTr("Backups, restaurações e varreduras desta sessão aparecem aqui.")
        }
    }
}
