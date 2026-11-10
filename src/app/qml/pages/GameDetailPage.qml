pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveVault.Backend

// One game, shown next to the library list: a banner with its art, the main
// actions (with their shortcuts), a few numbers, the named saves and the
// backup history.
Item {
    id: root

    property string gameId
    property var info: ({})
    property var history: []
    property var slotList: []
    property string activeSlot: ""

    signal back()

    // Actions shared with the window shortcuts (Ctrl+B, Ctrl+R, Ctrl+T).
    function backupNow() {
        if (gameId.length > 0 && !AppController.busy && AppController.vaultReady)
            AppController.backupGame(gameId)
    }
    function restoreNewest() {
        if (gameId.length > 0 && history.length > 0 && !AppController.busy)
            restoreDialog.openFor(history[0])
    }
    function switchSave() {
        if (gameId.length === 0 || AppController.busy || !AppController.vaultReady)
            return
        if (slotList.filter(slot => !slot.active).length > 0)
            switchMenu.open()
        else
            saveSlotDialog.openEmpty()
    }

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

    // Confirms deleting one backup.
    Popup {
        id: deleteDialog
        objectName: "deleteDialog"
        property var entry: ({})
        function openFor(snapshot) {
            entry = snapshot
            open()
        }
        modal: true
        focus: true
        width: 480
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
                text: qsTr("Apagar o backup de %1?").arg(deleteDialog.entry.date || "")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textSectionTitle
                font.weight: Font.Bold
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("O backup sai do histórico e não pode ser recuperado. Os saves no seu computador não mudam.")
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                spacing: Theme.space3
                Item { Layout.fillWidth: true }
                SecondaryButton {
                    text: qsTr("Cancelar")
                    onClicked: deleteDialog.close()
                }
                PrimaryButton {
                    text: qsTr("Apagar")
                    onClicked: {
                        AppController.deleteSnapshot(root.gameId, deleteDialog.entry.id)
                        deleteDialog.close()
                    }
                }
            }
        }
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
        anchors.centerIn: Overlay.overlay
        gameId: root.gameId
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + Theme.space8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: flick.width
            spacing: 0

            // --- Banner: the game's art (or its colour) with name and place ---
            Item {
                id: banner
                readonly property bool hasHero: (root.info.hero || "").toString().length > 0
                readonly property bool hasLogo: (root.info.logo || "").toString().length > 0
                readonly property bool protectedNow: root.history.length > 0
                readonly property color base: root.info.coverColor || Theme.surfaceRaised
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(200, Math.min(260, width * 0.36))

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: banner.base }
                        GradientStop { position: 1.0; color: Qt.darker(banner.base, 1.8) }
                    }
                }
                Image {
                    anchors.fill: parent
                    visible: banner.hasHero
                    source: banner.hasHero ? root.info.hero : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space6
                    visible: !banner.hasHero
                    text: root.initials(root.info.name)
                    color: Qt.rgba(1, 1, 1, 0.12)
                    font.family: Theme.fontDisplay
                    font.pixelSize: 120
                    font.weight: Font.Bold
                }
                // Darkens the bottom so the white text stays readable on any art.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.25; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.72) }
                    }
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space6
                    spacing: 6

                    Rectangle {
                        implicitWidth: protection.implicitWidth + 24
                        implicitHeight: 24
                        radius: 12
                        color: Qt.rgba(0, 0, 0, 0.5)
                        Row {
                            id: protection
                            anchors.centerIn: parent
                            spacing: 6
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 7
                                height: 7
                                radius: 4
                                color: banner.protectedNow ? "#4ADE80" : "#FBBF24"
                            }
                            Text {
                                text: banner.protectedNow
                                      ? qsTr("Protegido · backup %1").arg(root.history[0].relative)
                                      : qsTr("Ainda sem backup")
                                color: "#FFFFFF"
                                font.family: Theme.fontBody
                                font.pixelSize: Theme.textMeta
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Image {
                        Layout.preferredWidth: Math.min(320, banner.width * 0.5)
                        Layout.preferredHeight: 72
                        visible: banner.hasLogo
                        source: banner.hasLogo ? root.info.logo : ""
                        fillMode: Image.PreserveAspectFit
                        horizontalAlignment: Image.AlignLeft
                        verticalAlignment: Image.AlignBottom
                        asynchronous: true
                        mipmap: true
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !banner.hasLogo
                        text: root.info.name || ""
                        color: "#FFFFFF"
                        font.family: Theme.fontDisplay
                        font.pixelSize: 36
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [root.info.store || "", root.info.installDir || root.info.reason || ""]
                              .filter(part => part.length > 0).join("  ·  ")
                        color: Qt.rgba(1, 1, 1, 0.8)
                        font.family: Theme.fontBody
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.space6
                Layout.topMargin: Theme.space5
                spacing: Theme.space5

                // --- Main actions ------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    PrimaryButton {
                        text: qsTr("Fazer backup")
                        shortcutHint: "Ctrl B"
                        enabled: !AppController.busy && AppController.vaultReady
                        onClicked: root.backupNow()
                    }
                    SecondaryButton {
                        text: qsTr("Restaurar…")
                        shortcutHint: "Ctrl R"
                        enabled: !AppController.busy && root.history.length > 0
                        Accessible.description: qsTr("Restaura o backup mais recente. Para um backup mais antigo, use o histórico abaixo.")
                        onClicked: root.restoreNewest()
                    }
                    SecondaryButton {
                        id: switchButton
                        text: qsTr("Trocar save")
                        shortcutHint: "Ctrl T"
                        enabled: !AppController.busy && AppController.vaultReady
                        onClicked: root.switchSave()

                        ActionMenu {
                            id: switchMenu
                            y: switchButton.height + Theme.space1
                            model: root.slotList.filter(slot => !slot.active)
                                       .map(slot => ({ text: qsTr("Continuar “%1”").arg(slot.name), value: slot }))
                                       .concat([{ text: qsTr("Salvar o progresso atual como novo save…"), value: null }])
                            onTriggered: value => value ? restoreDialog.openForSlot(value) : saveSlotDialog.openEmpty()
                        }
                    }

                    Item { Layout.fillWidth: true }

                    IconButton {
                        id: openFolder
                        readonly property var folders: root.info.saveFolders || []
                        visible: folders.length > 0
                        iconPath: Icons.folder
                        text: folders.length > 1 ? qsTr("Pastas dos saves") : qsTr("Abrir pasta dos saves")
                        onClicked: {
                            if (folders.length === 1)
                                Qt.openUrlExternally(folders[0].url)
                            else
                                folderMenu.open()
                        }

                        ActionMenu {
                            id: folderMenu
                            x: openFolder.width - width
                            y: openFolder.height + Theme.space1
                            model: openFolder.folders.map(folder => ({ text: folder.path, value: folder.url }))
                            onTriggered: value => Qt.openUrlExternally(value)
                        }
                    }
                    IconButton {
                        visible: root.info.local === true
                        iconPath: Icons.trash
                        text: qsTr("Remover jogo adicionado à mão (os backups continuam no vault)")
                        enabled: !AppController.busy
                        onClicked: {
                            AppController.removeLocalGame(root.gameId)
                            root.back()
                        }
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

                // --- Numbers -----------------------------------------------
                GridLayout {
                    Layout.fillWidth: true
                    columns: width > 520 ? 4 : 2
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2

                    Repeater {
                        model: [
                            { label: qsTr("Último backup"), value: root.history.length > 0 ? root.history[0].relative : "—" },
                            { label: qsTr("Backups"), value: Format.number(root.history.length) },
                            { label: qsTr("Saves com nome"), value: Format.number(root.slotList.length) },
                            { label: qsTr("Fixados"), value: Format.number(root.history.filter(entry => entry.pinned === true).length) }
                        ]
                        delegate: Rectangle {
                            id: stat
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: statColumn.implicitHeight + 2 * Theme.space3
                            radius: Theme.radiusCard
                            color: Theme.card
                            border.color: Theme.border

                            ColumnLayout {
                                id: statColumn
                                anchors.fill: parent
                                anchors.margins: Theme.space3
                                anchors.leftMargin: Theme.space4
                                spacing: 2
                                Text {
                                    text: stat.modelData.label
                                    color: Theme.textSubtle
                                    font.family: Theme.fontBody
                                    font.pixelSize: Theme.textMeta
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: stat.modelData.value
                                    color: Theme.text
                                    font.family: Theme.fontMono
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                // --- Named saves (slots) -------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    RowLayout {
                        Layout.fillWidth: true
                        SectionLabel {
                            Layout.fillWidth: true
                            text: qsTr("Saves com nome")
                        }
                        SecondaryButton {
                            implicitHeight: 32
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
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        visible: root.slotList.length > 0
                        columns: Math.max(1, Math.floor((width + Theme.space2) / 220))
                        columnSpacing: Theme.space2
                        rowSpacing: Theme.space2

                        Repeater {
                            model: root.slotList
                            delegate: Rectangle {
                                id: slotCard
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: slotColumn.implicitHeight + 2 * Theme.space3
                                radius: Theme.radiusCard
                                color: modelData.active ? Theme.accentTint : Theme.card
                                border.width: modelData.active ? 2 : 1
                                border.color: modelData.active ? Theme.accent : Theme.border

                                ColumnLayout {
                                    id: slotColumn
                                    anchors.fill: parent
                                    anchors.margins: Theme.space3
                                    anchors.leftMargin: Theme.space4
                                    spacing: Theme.space1

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: slotCard.modelData.name
                                            color: Theme.text
                                            font.family: Theme.fontBody
                                            font.pixelSize: Theme.textBody
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            visible: slotCard.modelData.active
                                            text: qsTr("em uso")
                                            color: Theme.accentText
                                            font.family: Theme.fontBody
                                            font.pixelSize: Theme.textMeta
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Text {
                                        text: qsTr("Atualizado %1").arg(slotCard.modelData.relative)
                                        color: Theme.textSubtle
                                        font.family: Theme.fontBody
                                        font.pixelSize: Theme.textMeta
                                    }
                                    LinkButton {
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

                // --- History -------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    SectionLabel {
                        Layout.bottomMargin: Theme.space2
                        text: qsTr("Histórico de backups")
                    }

                    Repeater {
                        model: root.history
                        delegate: Item {
                            id: row
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: Math.max(48, rowLayout.implicitHeight + Theme.space2)

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: Theme.borderSoft
                            }

                            RowLayout {
                                id: rowLayout
                                anchors.fill: parent
                                spacing: Theme.space3

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: row.modelData.date + "  ·  " + row.modelData.relative
                                        color: Theme.text
                                        font.family: Theme.fontBody
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: [row.modelData.kindText,
                                               Format.count(row.modelData.files, qsTr("arquivo"), qsTr("arquivos")),
                                               (row.modelData.slot || "").length > 0 ? qsTr("save “%1”").arg(row.modelData.slot) : "",
                                               row.modelData.label.length > 0 ? "“" + row.modelData.label + "”" : ""]
                                              .filter(part => part.length > 0).join("  ·  ")
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
                                IconButton {
                                    implicitWidth: 34
                                    implicitHeight: 34
                                    iconPath: Icons.pin
                                    checkable: true
                                    checked: row.modelData.pinned === true
                                    text: checked ? qsTr("Desafixar backup") : qsTr("Fixar backup (a limpeza automática nunca o apaga)")
                                    enabled: !AppController.busy
                                    onClicked: AppController.setSnapshotPinned(root.gameId, row.modelData.id, checked)
                                }
                                IconButton {
                                    implicitWidth: 34
                                    implicitHeight: 34
                                    iconPath: Icons.trash
                                    text: qsTr("Apagar backup")
                                    enabled: !AppController.busy && row.modelData.pinned !== true
                                    onClicked: deleteDialog.openFor(row.modelData)
                                }
                                LinkButton {
                                    text: qsTr("Restaurar…")
                                    enabled: !AppController.busy
                                    onClicked: restoreDialog.openFor(row.modelData)
                                }
                            }
                        }
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        visible: root.history.length === 0
                        iconPath: Icons.backup
                        title: AppController.vaultReady ? qsTr("Nenhum backup ainda") : qsTr("Vault indisponível")
                        description: AppController.vaultReady
                                     ? qsTr("Faça o primeiro backup para poder restaurar este jogo depois.")
                                     : AppController.vaultError
                    }
                }
            }
        }
    }
}
