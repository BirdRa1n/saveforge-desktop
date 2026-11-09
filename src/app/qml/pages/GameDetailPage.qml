pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// One game: where it was found, its backup history, backup and restore.
Item {
    id: root

    property string gameId
    property var info: ({})
    property var history: []

    signal back()

    function reload() {
        info = gameId.length > 0 ? AppController.gameInfo(gameId) : ({})
        history = gameId.length > 0 ? AppController.snapshots(gameId) : []
    }

    function initials(value) {
        return (value || "").split(/\s+/).filter(w => w.length > 0).map(w => w[0]).join("").slice(0, 2).toUpperCase()
    }

    onGameIdChanged: reload()

    // Backups and restores update the library row; refresh this page then.
    Connections {
        target: AppController.library
        function onDataChanged() { root.reload() }
        function onModelReset() { root.reload() }
    }

    RestoreDialog {
        id: restoreDialog
        objectName: "restoreDialog"
        anchors.centerIn: parent
        gameId: root.gameId
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space8
        anchors.topMargin: 28
        spacing: Theme.space5

        SecondaryButton {
            text: qsTr("‹ Biblioteca")
            onClicked: root.back()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space5

            Rectangle {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 120
                radius: Theme.radiusCard
                color: root.info.coverColor || Theme.card

                Text {
                    anchors.centerIn: parent
                    text: root.initials(root.info.name)
                    color: Qt.rgba(1, 1, 1, 0.88)
                    font.family: Theme.fontDisplay
                    font.pixelSize: 40
                    font.weight: Font.Bold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: root.info.name || ""
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textPageTitle
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Text {
                    text: (root.info.store || "") + "  ·  " + (root.info.reason || "")
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                }
                Text {
                    Layout.fillWidth: true
                    visible: (root.info.installDir || "").length > 0
                    text: root.info.installDir || ""
                    color: Theme.textSubtle
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textMeta
                    elide: Text.ElideMiddle
                }
            }

            PrimaryButton {
                text: qsTr("Fazer backup agora")
                enabled: !AppController.busy && AppController.vaultReady
                onClicked: AppController.backupGame(root.gameId)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.info.launcherCloud === true
            implicitHeight: cloudRow.implicitHeight + 2 * Theme.space3
            radius: Theme.radiusControl
            color: Theme.warningBg

            RowLayout {
                id: cloudRow
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space3
                Icon { path: Icons.warning; color: Theme.warningFg }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("O cloud do launcher também sincroniza estes saves. Na restauração, você escolhe quem tem prioridade.")
                    color: Theme.warningFg
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    wrapMode: Text.Wrap
                }
            }
        }

        Text {
            text: qsTr("Histórico de backups")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textSectionTitle
            font.weight: Font.Bold
            Accessible.role: Accessible.Heading
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space2
            model: root.history
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                width: list.width
                implicitHeight: rowLayout.implicitHeight + 2 * Theme.space3
                radius: Theme.radiusControl
                color: Theme.card
                border.color: Theme.border

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    anchors.leftMargin: Theme.space4
                    spacing: Theme.space4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: row.modelData.date + "  ·  " + row.modelData.relative
                            color: Theme.text
                            font.family: Theme.fontBody
                            font.pixelSize: Theme.textBody
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Format.count(row.modelData.files, qsTr("arquivo"), qsTr("arquivos"))
                                  + (row.modelData.label.length > 0 ? "  ·  “" + row.modelData.label + "”" : "")
                            color: Theme.textSubtle
                            font.family: Theme.fontBody
                            font.pixelSize: Theme.textMeta
                            elide: Text.ElideRight
                        }
                    }
                    StatusBadge {
                        visible: row.modelData.inconsistent
                        text: qsTr("Arquivos mudaram durante a cópia")
                        kind: StatusBadge.Warning
                    }
                    StatusBadge {
                        text: row.modelData.kindText
                        kind: StatusBadge.Neutral
                    }
                    SecondaryButton {
                        text: qsTr("Restaurar…")
                        enabled: !AppController.busy
                        onClicked: restoreDialog.openFor(row.modelData)
                    }
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.history.length === 0
            iconPath: Icons.backup
            title: AppController.vaultReady ? qsTr("Nenhum backup ainda") : qsTr("Vault indisponível")
            description: AppController.vaultReady
                         ? qsTr("Faça o primeiro backup para poder restaurar este jogo depois.")
                         : AppController.vaultError
        }
    }
}
