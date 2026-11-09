pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// Library of detected games (AppController.library), with store filter and search.
Item {
    id: root

    signal gameSelected(string gameId)
    signal openSettings()

    LibraryFilterModel {
        id: filtered
        sourceModel: AppController.library
        search: searchField.text
    }

    AddGameDialog {
        id: addGameDialog
        objectName: "addGameDialog"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space8
        anchors.topMargin: 28
        spacing: 22

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space4

            Text {
                Layout.fillWidth: true
                text: qsTr("Biblioteca")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textPageTitle
                font.weight: Font.Bold
                font.letterSpacing: -0.6
                Accessible.role: Accessible.Heading
            }

            SearchInput {
                id: searchField
                Layout.preferredWidth: 300
                placeholderText: qsTr("Buscar jogos")
            }

            SecondaryButton {
                text: qsTr("Adicionar jogo")
                onClicked: addGameDialog.openEmpty()
            }

            SecondaryButton {
                text: qsTr("Escanear")
                enabled: !AppController.busy
                onClicked: AppController.refresh()
            }

            PrimaryButton {
                text: qsTr("Fazer backup de tudo")
                enabled: !AppController.busy && AppController.vaultReady && AppController.library.count > 0
                onClicked: AppController.backupAll()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

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

            Item { Layout.fillWidth: true }

            Spinner {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                running: AppController.busy
            }
            Text {
                visible: AppController.busy
                text: AppController.busyText
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
            }
            Text {
                visible: !AppController.busy
                text: Format.count(filtered.count, qsTr("jogo"), qsTr("jogos"))
                color: Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: filtered.count > 0
            clip: true
            cellWidth: Math.max(198, width / Math.max(1, Math.floor(width / 198)))
            cellHeight: 254
            model: filtered
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property string gameId
                required property string name
                required property string store
                required property string statusText
                required property int statusKind
                required property color coverColor
                width: grid.cellWidth
                height: grid.cellHeight

                GameCard {
                    anchors.fill: parent
                    anchors.rightMargin: 18
                    anchors.bottomMargin: 18
                    name: cell.name
                    store: cell.store
                    statusText: cell.statusText
                    statusKind: cell.statusKind
                    placeholderColor: cell.coverColor
                    onClicked: root.gameSelected(cell.gameId)
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: filtered.count === 0 && !(AppController.busy && AppController.profileCount === 0)
            title: AppController.profileCount === 0 ? qsTr("Nenhum perfil de jogo instalado")
                 : AppController.library.count === 0 ? qsTr("Nenhum jogo encontrado ainda")
                 : qsTr("Nenhum jogo corresponde ao filtro")
            description: AppController.profileCount === 0
                         ? qsTr("O SaveForge precisa dos perfis de jogos para saber onde ficam os saves. Procure ou importe perfis em Configurações, ou adicione um jogo à mão.")
                         : AppController.library.count === 0
                           ? qsTr("Nenhum jogo instalado ou save conhecido foi encontrado nesta máquina. Clique em Escanear depois de instalar ou jogar algo, ou adicione um jogo à mão.")
                           : qsTr("Tente outro termo de busca ou outra loja.")

            SecondaryButton {
                visible: AppController.profileCount === 0
                text: qsTr("Abrir Configurações")
                onClicked: root.openSettings()
            }
        }
    }
}
