pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// Library: the dense list of detected games on the left (store filter and the
// search from the title bar) and the selected game on the right
// (GameDetailPage). Arrow keys move the selection through the list.
Item {
    id: root

    // Game shown on the right; empty shows a hint to pick one.
    property string selectedGameId
    property alias search: filtered.search

    signal gameSelected(string gameId)
    signal openSettings()

    // Shortcuts of Main.qml act on the selected game through these.
    function backupSelected() { detail.backupNow() }
    function restoreSelected() { detail.restoreNewest() }
    function switchSaveSelected() { detail.switchSave() }
    function focusList() { list.forceActiveFocus() }

    // One row of the list, bound to the model roles of LibraryModel.
    component Cell: Item {
        id: cell
        required property int index
        required property string gameId
        required property string name
        required property string store
        required property string statusText
        required property int statusKind
        required property int snapshotCount
        required property color coverColor
        required property url cover
        required property bool iconOnly
        width: ListView.view.width
        height: row.implicitHeight

        GameRow {
            id: row
            anchors.fill: parent
            name: cell.name
            store: cell.store
            snapshotCount: cell.snapshotCount
            statusText: cell.statusText
            statusKind: cell.statusKind
            placeholderColor: cell.coverColor
            coverSource: cell.cover
            iconOnly: cell.iconOnly
            selected: cell.gameId === root.selectedGameId
            onClicked: {
                list.currentIndex = cell.index
                list.forceActiveFocus()
                root.gameSelected(cell.gameId)
            }
        }
    }

    LibraryFilterModel {
        id: filtered
        sourceModel: AppController.library
    }

    AddGameDialog {
        id: addGameDialog
        objectName: "addGameDialog"
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Game list -----------------------------------------------------
        Rectangle {
            Layout.preferredWidth: Math.max(380, Math.min(500, root.width * 0.42))
            Layout.fillHeight: true
            color: Theme.bg
            topLeftRadius: Theme.radiusPanel

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.space4
                spacing: Theme.space3

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space4
                    spacing: Theme.space1

                    Text {
                        text: qsTr("Biblioteca")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textPageTitle
                        font.weight: Font.Bold
                        Accessible.role: Accessible.Heading
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.space2
                        Layout.alignment: Qt.AlignBaseline
                        text: Format.count(filtered.count, qsTr("jogo"), qsTr("jogos"))
                        color: Theme.textSubtle
                        font.family: Theme.fontBody
                        font.pixelSize: Theme.textBody
                        elide: Text.ElideRight
                    }
                    IconButton {
                        iconPath: Icons.plus
                        text: qsTr("Adicionar jogo")
                        onClicked: addGameDialog.openEmpty()
                    }
                    IconButton {
                        iconPath: Icons.refresh
                        text: qsTr("Escanear de novo")
                        enabled: !AppController.busy
                        onClicked: AppController.refresh()
                    }
                    PrimaryButton {
                        Layout.leftMargin: Theme.space1
                        implicitHeight: 34
                        text: qsTr("Backup de tudo")
                        enabled: !AppController.busy && AppController.vaultReady && AppController.library.count > 0
                        onClicked: AppController.backupAll()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space5
                    spacing: 6

                    ButtonGroup { id: filterGroup }

                    Repeater {
                        model: [
                            { label: qsTr("Todos"), key: "" },
                            { label: "Steam", key: "steam" },
                            { label: "Epic", key: "epic" },
                            { label: "GOG", key: "gog" },
                            { label: qsTr("Outros"), key: "other" }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: filtered.store === modelData.key
                            ButtonGroup.group: filterGroup
                            onClicked: filtered.store = modelData.key
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                // List area: the list, the first-scan progress or an empty state.
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: list
                        objectName: "libraryList"
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space2
                        anchors.rightMargin: Theme.space2
                        visible: filtered.count > 0
                        clip: true
                        spacing: 2
                        model: filtered
                        currentIndex: -1
                        keyNavigationEnabled: true
                        boundsBehavior: Flickable.StopAtBounds
                        highlightMoveDuration: 0
                        ScrollBar.vertical: ScrollBar {}
                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Jogos")

                        // Keyboard moves pick the game, like a click.
                        onCurrentIndexChanged: {
                            if (activeFocus && currentItem)
                                root.gameSelected((currentItem as Cell).gameId)
                        }
                        Keys.onReturnPressed: root.backupSelected()

                        delegate: Cell {}
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        visible: filtered.count === 0 && AppController.busy
                        spacing: Theme.space3

                        Spinner {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            running: parent.visible
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: AppController.busyText
                            color: Theme.textMuted
                            font.family: Theme.fontBody
                            font.pixelSize: Theme.textBody
                        }
                    }

                    EmptyState {
                        anchors.fill: parent
                        anchors.margins: Theme.space5
                        visible: filtered.count === 0 && !AppController.busy
                        title: AppController.profileCount === 0 ? qsTr("Nenhum perfil de jogo instalado")
                             : AppController.library.count === 0 ? qsTr("Nenhum jogo encontrado ainda")
                             : qsTr("Nenhum jogo corresponde ao filtro")
                        description: AppController.profileCount === 0
                                     ? qsTr("O SaveForge precisa dos perfis de jogos para saber onde ficam os saves. Procure ou importe perfis em Configurações, ou adicione um jogo à mão.")
                                     : AppController.library.count === 0
                                       ? qsTr("Nenhum jogo instalado ou save conhecido foi encontrado nesta máquina. Escaneie de novo depois de instalar ou jogar algo, ou adicione um jogo à mão.")
                                       : qsTr("Tente outro termo de busca ou outra loja.")

                        SecondaryButton {
                            visible: AppController.profileCount === 0
                            text: qsTr("Abrir Configurações")
                            onClicked: root.openSettings()
                        }
                        SecondaryButton {
                            visible: AppController.library.count === 0
                            text: qsTr("Adicionar jogo")
                            onClicked: addGameDialog.openEmpty()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.border
        }

        // --- Selected game -------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bg

            GameDetailPage {
                id: detail
                objectName: "gameDetailPage"
                anchors.fill: parent
                visible: root.selectedGameId.length > 0
                gameId: root.selectedGameId
                onBack: root.gameSelected("")
            }

            EmptyState {
                anchors.fill: parent
                anchors.margins: Theme.space8
                visible: root.selectedGameId.length === 0
                iconPath: Icons.gamepad
                title: qsTr("Escolha um jogo")
                description: qsTr("Selecione um jogo na lista para ver os backups, trocar de save ou restaurar. Use as setas para navegar e Ctrl+K para buscar.")
            }
        }
    }
}
