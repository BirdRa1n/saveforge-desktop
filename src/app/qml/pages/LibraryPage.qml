pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Biblioteca de jogos. O modelo de jogos virá da camada services (tarefa D1.9);
// enquanto estiver vazio, a página mostra o estado vazio.
Item {
    id: root

    property var games: []
    property bool scanAvailable: false

    signal scanRequested()
    signal backupAllRequested()

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
                id: search
                Layout.preferredWidth: 300
                placeholderText: qsTr("Buscar jogos")
            }

            SecondaryButton {
                text: qsTr("Escanear")
                enabled: root.scanAvailable
                onClicked: root.scanRequested()
            }

            PrimaryButton {
                text: qsTr("Fazer backup de tudo")
                enabled: root.games.length > 0
                onClicked: root.backupAllRequested()
            }
        }

        RowLayout {
            spacing: Theme.space2

            ButtonGroup { id: filterGroup }

            Repeater {
                model: [qsTr("Todos"), "Steam", "Epic", "GOG", qsTr("Manuais")]
                delegate: FilterChip {
                    required property string modelData
                    required property int index
                    text: modelData
                    checked: index === 0
                    ButtonGroup.group: filterGroup
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.games.length > 0
            clip: true
            cellWidth: Math.max(198, width / Math.max(1, Math.floor(width / 198)))
            cellHeight: 254
            model: root.games
            delegate: Item {
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight

                id: cell
                GameCard {
                    anchors.fill: parent
                    anchors.rightMargin: 18
                    anchors.bottomMargin: 18
                    name: cell.modelData.name
                    store: cell.modelData.store
                    statusText: cell.modelData.statusText
                    statusKind: cell.modelData.statusKind
                    placeholderColor: cell.modelData.color
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignCenter
            visible: root.games.length === 0
            title: qsTr("Nenhum jogo encontrado ainda")
            description: qsTr("O SaveForge vai procurar jogos da Steam, Epic e GOG, além dos locais de save conhecidos. Você também poderá adicionar jogos manualmente.")
        }
    }
}
