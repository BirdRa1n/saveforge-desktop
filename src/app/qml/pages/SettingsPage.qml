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

    FileDialog {
        id: profileDialog
        title: qsTr("Escolha um arquivo de perfil")
        nameFilters: [qsTr("Perfis de jogo (*.json)")]
        onAccepted: profileError.text = AppController.importProfile(selectedFile)
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
                title: qsTr("Aparência")
                Paragraph { text: qsTr("Tema e cor de destaque da interface. A cor aparece nos botões principais, na seleção e no save em uso.") }

                RowLayout {
                    spacing: 6
                    ButtonGroup { id: themeGroup }
                    Repeater {
                        model: [
                            { label: qsTr("Sistema"), key: "system" },
                            { label: qsTr("Escuro"), key: "dark" },
                            { label: qsTr("Claro"), key: "light" }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: AppController.themeMode === modelData.key
                            ButtonGroup.group: themeGroup
                            onClicked: AppController.themeMode = modelData.key
                        }
                    }
                }

                RowLayout {
                    Layout.topMargin: Theme.space1
                    spacing: Theme.space3
                    Repeater {
                        model: [
                            { color: "#22D3EE", name: qsTr("Ciano") },
                            { color: "#3B82F6", name: qsTr("Azul") },
                            { color: "#8B5CF6", name: qsTr("Violeta") },
                            { color: "#22C55E", name: qsTr("Verde") },
                            { color: "#F97316", name: qsTr("Laranja") },
                            { color: "#EC4899", name: qsTr("Rosa") }
                        ]
                        delegate: AbstractButton {
                            id: swatch
                            required property var modelData
                            readonly property bool current: AppController.accentColor.toUpperCase() === modelData.color
                            implicitWidth: 32
                            implicitHeight: 32
                            focusPolicy: Qt.StrongFocus
                            hoverEnabled: true
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: qsTr("Cor de destaque %1").arg(modelData.name)
                            Accessible.checked: current
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: modelData.name
                            onClicked: AppController.accentColor = modelData.color

                            background: Rectangle {
                                radius: width / 2
                                color: "transparent"
                                border.width: swatch.current || swatch.visualFocus ? 2 : 0
                                border.color: swatch.modelData.color
                            }
                            contentItem: Item {
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: swatch.current || swatch.visualFocus ? 22 : 28
                                    height: width
                                    radius: width / 2
                                    color: swatch.modelData.color
                                }
                            }
                        }
                    }
                }
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
                    color: Theme.dangerFg
                    text: AppController.vaultError
                }
                SecondaryButton {
                    text: qsTr("Alterar pasta…")
                    enabled: !AppController.busy
                    onClicked: vaultDialog.open()
                }
            }

            Section {
                title: qsTr("Backups automáticos")
                Paragraph {
                    text: qsTr("O SaveForge pode guardar o progresso sozinho. Ele só cria um backup quando os saves mudaram desde o último.")
                }
                ToggleSetting {
                    Layout.fillWidth: true
                    text: qsTr("Fazer backup quando um jogo fechar")
                    checked: AppController.backupOnGameExit
                    onToggled: AppController.backupOnGameExit = checked
                }
                Paragraph {
                    text: qsTr("Backup periódico dos jogos com saves novos:")
                }
                RowLayout {
                    spacing: Theme.space2
                    ButtonGroup { id: intervalGroup }
                    Repeater {
                        model: [
                            { hours: 0, label: qsTr("Desligado") },
                            { hours: 1, label: qsTr("A cada hora") },
                            { hours: 6, label: qsTr("A cada 6 horas") },
                            { hours: 24, label: qsTr("Uma vez por dia") }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: AppController.backupIntervalHours === modelData.hours
                            ButtonGroup.group: intervalGroup
                            onClicked: AppController.backupIntervalHours = modelData.hours
                        }
                    }
                }
                ToggleSetting {
                    Layout.fillWidth: true
                    visible: AppController.trayAvailable
                    text: qsTr("Continuar na bandeja do sistema ao fechar a janela, para os backups automáticos seguirem funcionando")
                    checked: AppController.runInBackground
                    onToggled: AppController.runInBackground = checked
                }
            }

            Section {
                title: qsTr("Espaço e manutenção")
                Paragraph {
                    text: qsTr("Escolha quantos backups guardar. Backups fixados, o estado atual de cada save com nome e o ponto de desfazer da última restauração nunca são apagados.")
                }
                Paragraph {
                    text: qsTr("Manter de cada jogo:")
                }
                RowLayout {
                    spacing: Theme.space2
                    ButtonGroup { id: keepLastGroup }
                    Repeater {
                        model: [
                            { count: 0, label: qsTr("Todos os backups") },
                            { count: 10, label: qsTr("Os 10 mais recentes") },
                            { count: 30, label: qsTr("Os 30 mais recentes") },
                            { count: 100, label: qsTr("Os 100 mais recentes") }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: AppController.retentionKeepLast === modelData.count
                            ButtonGroup.group: keepLastGroup
                            onClicked: AppController.retentionKeepLast = modelData.count
                        }
                    }
                }
                Paragraph {
                    visible: AppController.retentionKeepLast > 0
                    text: qsTr("E também tudo o que for mais novo que:")
                }
                RowLayout {
                    visible: AppController.retentionKeepLast > 0
                    spacing: Theme.space2
                    ButtonGroup { id: keepDaysGroup }
                    Repeater {
                        model: [
                            { days: 0, label: qsTr("Nada além disso") },
                            { days: 30, label: qsTr("30 dias") },
                            { days: 90, label: qsTr("90 dias") },
                            { days: 365, label: qsTr("1 ano") }
                        ]
                        delegate: FilterChip {
                            required property var modelData
                            text: modelData.label
                            checked: AppController.retentionKeepDays === modelData.days
                            ButtonGroup.group: keepDaysGroup
                            onClicked: AppController.retentionKeepDays = modelData.days
                        }
                    }
                }
                Paragraph {
                    text: AppController.maintenanceText + " " + qsTr("O SaveForge confere o vault e libera espaço uma vez por semana.")
                    color: Theme.textSubtle
                }
                RowLayout {
                    spacing: Theme.space3
                    SecondaryButton {
                        text: qsTr("Limpar agora")
                        enabled: !AppController.busy && AppController.vaultReady
                        onClicked: AppController.cleanUpVault()
                    }
                    SecondaryButton {
                        text: qsTr("Verificar o vault")
                        enabled: !AppController.busy && AppController.vaultReady
                        onClicked: AppController.verifyVault()
                    }
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
                Paragraph {
                    text: qsTr("Perfis locais têm prioridade sobre os do Registry. Use-os para jogos que você adicionou à mão ou para testar um perfil antes de contribuir com ele.")
                    color: Theme.textSubtle
                }
                SecondaryButton {
                    text: qsTr("Importar perfil local…")
                    enabled: !AppController.busy
                    onClicked: {
                        profileError.text = ""
                        profileDialog.open()
                    }
                }
                Paragraph {
                    id: profileError
                    visible: text.length > 0
                    color: Theme.warningFg
                }
            }

            Section {
                title: qsTr("Capas dos jogos")
                Paragraph {
                    text: qsTr("O SaveForge usa as capas que a Steam e a GOG já guardam neste computador. Para os jogos sem capa local, ele pode baixar a capa do servidor de imagens da Steam. O pedido informa só o número do jogo na loja.")
                }
                ToggleSetting {
                    Layout.fillWidth: true
                    text: qsTr("Baixar as capas que faltam")
                    checked: AppController.downloadCovers
                    onToggled: AppController.downloadCovers = checked
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
