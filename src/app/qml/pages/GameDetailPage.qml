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
    property var slotList: []
    property string activeSlot: ""

    signal back()

    function reload() {
        info = gameId.length > 0 ? AppController.gameInfo(gameId) : ({})
        history = gameId.length > 0 ? AppController.snapshots(gameId) : []
        slotList = gameId.length > 0 ? AppController.saveSlots(gameId) : []
        activeSlot = gameId.length > 0 ? AppController.activeSlot(gameId) : ""
    }

    Connections {
        target: AppController
        function onSlotsChanged(changedId) {
            if (changedId === root.gameId)
                root.reload()
        }
    }

    SaveSlotDialog {
        id: saveSlotDialog
        objectName: "saveSlotDialog"
        gameId: root.gameId
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

        // Banner with the game's logo, when the launcher has them (Steam).
        Item {
            id: banner
            readonly property bool hasHero: (root.info.hero || "").toString().length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: hasHero ? Math.min(240, width * 0.28) : 0
            visible: hasHero

            RoundedImage {
                id: hero
                anchors.fill: parent
                source: banner.hasHero ? root.info.hero : ""
            }
            // Fades the banner into the page so the logo stays readable.
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusCard
                gradient: Gradient {
                    GradientStop { position: 0.4; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.bg }
                }
            }
            Image {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space6
                width: Math.min(360, parent.width * 0.4)
                height: parent.height * 0.42
                source: (root.info.logo || "").toString().length > 0 ? root.info.logo : ""
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignLeft
                verticalAlignment: Image.AlignBottom
                asynchronous: true
                smooth: true
                mipmap: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space5

            // Cover: landscape art, the store icon on a colour tile, or initials.
            // Hidden under a banner, which already shows the game's art and logo.
            Rectangle {
                readonly property bool hasCover: (root.info.cover || "").toString().length > 0
                readonly property bool landscape: hasCover && root.info.iconOnly !== true
                visible: !banner.hasHero
                Layout.preferredWidth: landscape ? 230 : 108
                Layout.preferredHeight: 108
                radius: Theme.radiusCard
                color: root.info.coverColor || Theme.card

                Text {
                    anchors.centerIn: parent
                    visible: !parent.hasCover
                    text: root.initials(root.info.name)
                    color: Qt.rgba(1, 1, 1, 0.88)
                    font.family: Theme.fontDisplay
                    font.pixelSize: 40
                    font.weight: Font.Bold
                }
                RoundedImage {
                    anchors.fill: parent
                    visible: parent.landscape
                    source: parent.landscape ? root.info.cover : ""
                }
                Image {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    visible: parent.hasCover && !parent.landscape
                    source: visible ? root.info.cover : ""
                    sourceSize: Qt.size(112, 112)
                    fillMode: Image.PreserveAspectFit
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
                    Layout.fillWidth: true
                    text: (root.info.store || "") + "  ·  " + (root.info.reason || "")
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    elide: Text.ElideRight
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
                // Secondary actions sit under the title so the header never
                // grows wider than the page.
                RowLayout {
                    Layout.topMargin: Theme.space2
                    spacing: Theme.space2

                    SecondaryButton {
                        id: openFolder
                        readonly property var folders: root.info.saveFolders || []
                        visible: folders.length > 0
                        iconPath: Icons.folder
                        text: folders.length > 1 ? qsTr("Pastas dos saves") : qsTr("Abrir pasta dos saves")
                        Accessible.description: qsTr("Abre no gerenciador de arquivos a pasta onde o jogo guarda os saves.")
                        onClicked: {
                            if (folders.length === 1)
                                Qt.openUrlExternally(folders[0].url)
                            else
                                folderMenu.open()
                        }

                        ActionMenu {
                            id: folderMenu
                            y: openFolder.height + Theme.space1
                            model: openFolder.folders.map(folder => ({ text: folder.path, value: folder.url }))
                            onTriggered: value => Qt.openUrlExternally(value)
                        }
                    }

                    SecondaryButton {
                        visible: root.info.local === true
                        text: qsTr("Remover jogo")
                        Accessible.description: qsTr("Remove o jogo adicionado à mão. Os backups continuam no vault.")
                        enabled: !AppController.busy
                        onClicked: {
                            AppController.removeLocalGame(root.gameId)
                            root.back()
                        }
                    }
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

        // Save slots: named lines of progress the user switches between.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Saves")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textSectionTitle
                    font.weight: Font.Bold
                    Accessible.role: Accessible.Heading
                }
                SecondaryButton {
                    text: qsTr("Salvar como novo save…")
                    enabled: !AppController.busy && AppController.vaultReady
                    onClicked: saveSlotDialog.openEmpty()
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.slotList.length === 0
                      ? qsTr("Dê um nome ao progresso atual para manter saves separados, como outro personagem ou uma partida de teste. Você troca entre eles quando quiser, e cada um continua de onde parou.")
                      : root.activeSlot.length > 0
                        ? qsTr("Em uso: “%1”. Os backups deste jogo atualizam o save em uso.").arg(root.activeSlot)
                        : qsTr("O progresso atual não tem nome. Para poder voltar a ele depois de trocar de save, salve-o como um novo save.")
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }

            Flow {
                Layout.fillWidth: true
                visible: root.slotList.length > 0
                spacing: Theme.space3

                Repeater {
                    model: root.slotList
                    delegate: Rectangle {
                        id: slotCard
                        required property var modelData
                        width: 260
                        // Same height with or without the button, so cards line up.
                        implicitHeight: Math.max(slotColumn.implicitHeight + 2 * Theme.space4, 136)
                        radius: Theme.radiusCard
                        color: Theme.card
                        border.width: modelData.active ? 2 : 1
                        border.color: modelData.active ? Theme.accent : Theme.border

                        ColumnLayout {
                            id: slotColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.space4
                            spacing: Theme.space2

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: slotCard.modelData.name
                                    color: Theme.text
                                    font.family: Theme.fontBody
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                StatusBadge {
                                    visible: slotCard.modelData.active
                                    text: qsTr("Em uso")
                                    kind: StatusBadge.Success
                                }
                            }
                            Text {
                                text: qsTr("Atualizado %1").arg(slotCard.modelData.relative)
                                color: Theme.textSubtle
                                font.family: Theme.fontBody
                                font.pixelSize: Theme.textMeta
                            }
                            SecondaryButton {
                                Layout.topMargin: Theme.space1
                                visible: !slotCard.modelData.active
                                text: qsTr("Continuar este save")
                                enabled: !AppController.busy
                                onClicked: restoreDialog.openForSlot(slotCard.modelData)
                            }
                        }
                    }
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
                        visible: (row.modelData.slot || "").length > 0
                        text: row.modelData.slot || ""
                        kind: StatusBadge.Neutral
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
