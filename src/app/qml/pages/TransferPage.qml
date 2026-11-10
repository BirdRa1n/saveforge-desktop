pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SaveVault.Backend

// Moving backups between computers as .gamesave files: export the chosen
// games, or import a file (which adds backups and never restores).
Item {
    id: root

    property var games: []
    property var selected: ({})
    property var previous: ({ exists: false })
    property var preview: ({})
    property url importFile

    readonly property int selectedCount: Object.keys(selected).filter(id => selected[id]).length

    function reload() {
        games = AppController.exportableGames()
        previous = AppController.lastExport()
        const kept = {}
        for (const game of games)
            kept[game.id] = selected[game.id] === true
        selected = kept
    }

    function setAll(value) {
        const all = {}
        for (const game of games)
            all[game.id] = value
        selected = all
    }

    Component.onCompleted: reload()
    onVisibleChanged: if (visible) reload()

    Connections {
        target: AppController.library
        function onModelReset() { root.reload() }
        function onDataChanged() { root.reload() }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Salvar exportação")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "gamesave"
        nameFilters: [qsTr("Saves do SaveVault (*.gamesave)")]
        currentFile: "SaveVault-" + new Date().toISOString().slice(0, 10) + ".gamesave"
        onAccepted: AppController.exportGames(root.games.filter(g => root.selected[g.id]).map(g => g.id), selectedFile,
                                              historyOption.checked, incrementalOption.checked && root.previous.exists,
                                              foldersOption.checked)
    }

    FileDialog {
        id: importDialog
        title: qsTr("Escolher arquivo .gamesave")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Saves do SaveVault (*.gamesave)")]
        onAccepted: {
            root.importFile = selectedFile
            root.preview = AppController.inspectGamesave(selectedFile)
            importPreview.open()
        }
    }

    component Section: Rectangle {
        id: section
        default property alias content: sectionColumn.data
        property string title
        Layout.fillWidth: true
        implicitHeight: sectionColumn.implicitHeight + 2 * Theme.space5
        radius: Theme.radiusCard
        color: Theme.card
        border.color: Theme.border

        ColumnLayout {
            id: sectionColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space5
            spacing: Theme.space3

            Text {
                text: section.title
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textSectionTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.Heading
            }
        }
    }

    component Paragraph: Text {
        Layout.fillWidth: true
        color: Theme.textMuted
        font.family: Theme.fontBody
        font.pixelSize: Theme.textBody
        wrapMode: Text.Wrap
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * Theme.space8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            x: Theme.space8
            y: 28
            width: Math.min(820, parent.width - 2 * Theme.space8)
            spacing: Theme.space5

            Text {
                text: qsTr("Importar / Exportar")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textPageTitle
                font.weight: Font.Bold
                font.letterSpacing: -0.6
                Accessible.role: Accessible.Heading
            }

            Section {
                title: qsTr("Exportar")
                Paragraph {
                    text: qsTr("Salve os backups num arquivo .gamesave para levar a outro computador, guardar num pendrive ou mandar para alguém. O arquivo é um ZIP comum, que qualquer programa abre.")
                }

                Paragraph {
                    visible: root.games.length === 0
                    color: Theme.textSubtle
                    text: qsTr("Nenhum jogo tem backup ainda. Faça um backup na Biblioteca para poder exportar.")
                }

                RowLayout {
                    visible: root.games.length > 0
                    spacing: Theme.space2
                    SecondaryButton {
                        text: qsTr("Selecionar todos")
                        onClicked: root.setAll(true)
                    }
                    SecondaryButton {
                        text: qsTr("Limpar seleção")
                        enabled: root.selectedCount > 0
                        onClicked: root.setAll(false)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: root.games.length > 0
                    columns: 2
                    columnSpacing: Theme.space3
                    rowSpacing: 0

                    Repeater {
                        model: root.games
                        delegate: CheckItem {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.name
                            detail: Format.count(modelData.snapshots, qsTr("backup"), qsTr("backups"))
                            checked: root.selected[modelData.id] === true
                            onToggled: {
                                const copy = Object.assign({}, root.selected)
                                copy[modelData.id] = checked
                                root.selected = copy
                            }
                        }
                    }
                }

                ToggleSetting {
                    id: historyOption
                    Layout.fillWidth: true
                    visible: root.games.length > 0
                    text: qsTr("Incluir todo o histórico (sem isso, só o backup mais recente de cada jogo)")
                }
                ToggleSetting {
                    id: incrementalOption
                    Layout.fillWidth: true
                    visible: root.games.length > 0 && root.previous.exists === true
                    text: qsTr("Só o que mudou desde a última exportação (%1). Quem importar precisa ter importado aquela antes.")
                          .arg(root.previous.date || "")
                }
                ToggleSetting {
                    id: foldersOption
                    Layout.fillWidth: true
                    visible: root.games.length > 0
                    text: qsTr("Incluir o caminho das minhas pastas (ajuda a diagnosticar problemas, mas mostra o nome do seu usuário)")
                }

                PrimaryButton {
                    visible: root.games.length > 0
                    text: root.selectedCount > 0
                          ? qsTr("Exportar %1…").arg(Format.count(root.selectedCount, qsTr("jogo"), qsTr("jogos")))
                          : qsTr("Exportar…")
                    enabled: root.selectedCount > 0 && !AppController.busy && AppController.vaultReady
                    onClicked: exportDialog.open()
                }
            }

            Section {
                title: qsTr("Importar")
                Paragraph {
                    text: qsTr("Adicione ao seu histórico os backups de um arquivo .gamesave. Nada é sobrescrito: depois você escolhe, na página de cada jogo, qual backup restaurar.")
                }
                SecondaryButton {
                    iconPath: Icons.folder
                    text: qsTr("Escolher arquivo .gamesave…")
                    enabled: !AppController.busy && AppController.vaultReady
                    onClicked: importDialog.open()
                }
            }
        }
    }

    // Shows what a .gamesave holds before importing it.
    Popup {
        id: importPreview
        objectName: "importPreview"
        modal: true
        focus: true
        width: 520
        anchors.centerIn: Overlay.overlay
        padding: Theme.space6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusCard
            color: Theme.surface
            border.color: Theme.borderStrong
        }
        Overlay.modal: Rectangle { color: Theme.scrim }

        contentItem: ColumnLayout {
            spacing: Theme.space4

            Text {
                Layout.fillWidth: true
                text: root.preview.ok === true ? qsTr("Importar backups") : qsTr("Não foi possível abrir o arquivo")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textSectionTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.Heading
            }
            Paragraph {
                visible: root.preview.ok !== true
                text: root.preview.error || ""
            }
            Paragraph {
                visible: root.preview.ok === true
                text: qsTr("Exportado em %1.").arg(root.preview.date || "")
            }
            Repeater {
                model: root.preview.ok === true ? root.preview.games : []
                delegate: Text {
                    required property var modelData
                    Layout.fillWidth: true
                    text: "•  " + modelData.name + "  ·  " + Format.count(modelData.snapshots, qsTr("backup"), qsTr("backups"))
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.fillWidth: true
                visible: root.preview.missingBase === true
                implicitHeight: baseWarning.implicitHeight + 2 * Theme.space3
                radius: Theme.radiusControl
                color: Theme.warningBg
                Text {
                    id: baseWarning
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    text: qsTr("Esta é uma exportação incremental: importe antes a exportação anterior, de onde ela continua.")
                    color: Theme.warningFg
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    wrapMode: Text.Wrap
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                spacing: Theme.space3
                Item { Layout.fillWidth: true }
                SecondaryButton {
                    text: root.preview.ok === true ? qsTr("Cancelar") : qsTr("Fechar")
                    onClicked: importPreview.close()
                }
                PrimaryButton {
                    visible: root.preview.ok === true
                    text: qsTr("Importar")
                    enabled: root.preview.missingBase !== true && !AppController.busy
                    onClicked: {
                        AppController.importGamesave(root.importFile)
                        importPreview.close()
                    }
                }
            }
        }
    }
}
