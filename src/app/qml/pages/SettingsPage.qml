pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SaveForge.Backend

// Vault location, game profiles (registry) and the launcher-cloud policy.
Item {
    id: root

    FolderDialog {
        id: vaultDialog
        title: qsTr("Escolha a pasta do vault")
        onAccepted: vaultError.text = AppController.setVaultFolder(selectedFolder)
    }

    FolderDialog {
        id: registryDialog
        title: qsTr("Escolha a pasta da distribuição de perfis")
        onAccepted: AppController.importRegistry(selectedFolder)
    }

    component Section: Rectangle {
        id: section
        property string title
        default property alias content: body.data
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + 2 * Theme.space5
        radius: Theme.radiusCard
        color: Theme.card
        border.color: Theme.border

        ColumnLayout {
            id: body
            anchors.fill: parent
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
                text: qsTr("Configurações")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textPageTitle
                font.weight: Font.Bold
                font.letterSpacing: -0.6
                Accessible.role: Accessible.Heading
            }

            Section {
                title: qsTr("Vault")
                Paragraph { text: qsTr("Pasta onde os backups são guardados. Pode ficar em outro disco ou numa pasta sincronizada com a nuvem.") }
                Text {
                    Layout.fillWidth: true
                    text: AppController.vaultPath
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    elide: Text.ElideMiddle
                }
                Paragraph {
                    id: vaultError
                    visible: text.length > 0
                    color: "#F87171"
                    text: AppController.vaultError
                }
                SecondaryButton {
                    text: qsTr("Alterar pasta…")
                    enabled: !AppController.busy
                    onClicked: vaultDialog.open()
                }
            }

            Section {
                title: qsTr("Perfis de jogos")
                Paragraph {
                    text: qsTr("Perfis disponíveis: %1. Os perfis dizem ao SaveForge onde cada jogo guarda os saves.")
                          .arg(Format.number(AppController.profileCount))
                }
                Rectangle {
                    Layout.fillWidth: true
                    visible: AppController.usingTestKeys
                    implicitHeight: testKeys.implicitHeight + 2 * Theme.space3
                    radius: Theme.radiusControl
                    color: Theme.warningBg
                    Text {
                        id: testKeys
                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        text: qsTr("Versão de desenvolvimento: esta compilação aceita perfis assinados com chaves de teste públicas.")
                        color: Theme.warningFg
                        font.family: Theme.fontBody
                        font.pixelSize: Theme.textBody
                        wrapMode: Text.Wrap
                    }
                }
                Repeater {
                    model: AppController.notices
                    delegate: Paragraph {
                        required property string modelData
                        text: modelData
                        color: Theme.textSubtle
                        font.pixelSize: Theme.textMeta
                    }
                }
                Paragraph {
                    text: AppController.canUpdateRegistry
                          ? AppController.registryCheckedText
                          : qsTr("Esta versão não tem um servidor de perfis configurado. Para adicionar perfis, importe uma distribuição.")
                    color: Theme.textSubtle
                }
                ToggleSetting {
                    Layout.fillWidth: true
                    visible: AppController.canUpdateRegistry
                    text: qsTr("Procurar novos perfis automaticamente, uma vez por dia")
                    checked: AppController.autoUpdateRegistry
                    onToggled: AppController.autoUpdateRegistry = checked
                }
                RowLayout {
                    spacing: Theme.space3
                    SecondaryButton {
                        visible: AppController.canUpdateRegistry
                        text: qsTr("Procurar agora")
                        enabled: !AppController.busy
                        onClicked: AppController.updateRegistry()
                    }
                    SecondaryButton {
                        text: qsTr("Importar distribuição de perfis…")
                        enabled: !AppController.busy
                        onClicked: registryDialog.open()
                    }
                }
            }

            Section {
                title: qsTr("Cloud dos launchers")
                Paragraph {
                    text: qsTr("Quando um launcher (como a Steam) também sincroniza os saves de um jogo, ele pode sobrescrever uma restauração. Escolha o comportamento padrão; você ainda pode mudar a decisão em cada restauração.")
                }
                RowLayout {
                    spacing: Theme.space2
                    ButtonGroup { id: policyGroup }
                    Repeater {
                        model: [
                            { key: "ask", label: qsTr("Perguntar sempre") },
                            { key: "prefer-launcher", label: qsTr("Priorizar o launcher") },
                            { key: "prefer-saveforge", label: qsTr("Priorizar o SaveForge") }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: AppController.cloudPolicy === modelData.key
                            ButtonGroup.group: policyGroup
                            onClicked: AppController.cloudPolicy = modelData.key
                        }
                    }
                }
            }
        }
    }
}
