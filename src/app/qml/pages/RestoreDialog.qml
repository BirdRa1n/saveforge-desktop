pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// Shows what a restore would do and asks for confirmation. Nothing is written
// before the user confirms; the controller then executes the reviewed plan.
// Switching to a save slot is the restore of the slot's newest snapshot
// (openForSlot), with wording about saves instead of backups.
Popup {
    id: root

    property string gameId
    property var snapshot: ({})
    property var plan: ({})
    // "saveforge" or "launcher" when the launcher's cloud also syncs the saves.
    property string cloudChoice: ""
    // Name of the save slot being switched to; empty for a plain restore.
    property string switchTo: ""
    readonly property bool switching: switchTo.length > 0

    // Switching also works when the files already match: it then only changes
    // which save is in use.
    readonly property bool canRestore: plan.ok === true && plan.gameRunning !== true
                                       && (plan.hasChanges === true || switching)
                                       && (plan.cloudConflict !== true || cloudChoice === "saveforge"
                                           || plan.hasChanges !== true)

    function openFor(entry) {
        switchTo = ""
        preparePlan(entry)
    }

    // slot: an entry of AppController.saveSlots()
    function openForSlot(slot) {
        switchTo = slot.name
        preparePlan({ id: slot.snapshotId, date: slot.date })
    }

    function preparePlan(entry) {
        filesToggle.expanded = false
        snapshot = entry
        plan = AppController.planRestore(gameId, entry.id)
        cloudChoice = AppController.cloudPolicy === "prefer-saveforge" ? "saveforge"
                    : AppController.cloudPolicy === "prefer-launcher" ? "launcher" : ""
        open()
    }

    modal: true
    focus: true
    width: 560
    padding: Theme.space6
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.surface
        border.color: Theme.borderStrong
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

    contentItem: ColumnLayout {
        spacing: Theme.space4

        Text {
            Layout.fillWidth: true
            text: root.switching ? qsTr("Trocar para o save “%1”").arg(root.switchTo)
                                 : qsTr("Restaurar backup de %1").arg(root.snapshot.date || "")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textSectionTitle
            font.weight: Font.Bold
            wrapMode: Text.Wrap
            Accessible.role: Accessible.Heading
        }

        Text {
            Layout.fillWidth: true
            visible: root.plan.ok !== true
            text: root.plan.error || ""
            color: Theme.dangerFg
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            wrapMode: Text.Wrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.plan.ok === true
            spacing: Theme.space2

            Repeater {
                model: [
                    { n: root.plan.overwrite || 0, one: qsTr("arquivo será substituído"), many: qsTr("arquivos serão substituídos") },
                    { n: root.plan.create || 0, one: qsTr("arquivo será criado"), many: qsTr("arquivos serão criados") },
                    { n: root.plan.unchanged || 0, one: qsTr("arquivo já está igual"), many: qsTr("arquivos já estão iguais") },
                    { n: root.plan.skip || 0, one: qsTr("arquivo não pode ser restaurado nesta máquina"), many: qsTr("arquivos não podem ser restaurados nesta máquina") },
                    { n: root.plan.extra || 0, one: qsTr("arquivo atual não está no backup e será mantido"), many: qsTr("arquivos atuais não estão no backup e serão mantidos") },
                    { n: root.plan.registry || 0, one: qsTr("chave do registro do Windows será restaurada"), many: qsTr("chaves do registro do Windows serão restauradas") },
                    { n: root.plan.registrySkipped || 0, one: qsTr("chave do registro do Windows não pode ser restaurada neste sistema"), many: qsTr("chaves do registro do Windows não podem ser restauradas neste sistema") }
                ]
                delegate: Text {
                    required property var modelData
                    visible: modelData.n > 0
                    text: "•  " + Format.count(modelData.n, modelData.one, modelData.many)
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.plan.hasChanges === false
                text: root.switching ? qsTr("Os arquivos atuais já são iguais a este save. Só o save em uso vai mudar.")
                                     : qsTr("Os arquivos atuais já são iguais a este backup. Não há nada para restaurar.")
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }

            // File by file: what happens to each one, with backup and current sizes.
            Text {
                id: filesToggle
                objectName: "filesToggle"
                property bool expanded: false
                visible: (root.plan.files || []).length > 0
                text: (expanded ? "▾  " : "▸  ") + qsTr("Ver arquivos")
                color: Theme.accentSoft
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Button
                Accessible.name: expanded ? qsTr("Esconder arquivos") : qsTr("Ver arquivos")
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: filesToggle.expanded = !filesToggle.expanded
                }
            }

            ListView {
                id: fileList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 220)
                visible: filesToggle.expanded && filesToggle.visible
                clip: true
                spacing: 2
                model: filesToggle.expanded ? root.plan.files : []
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: RowLayout {
                    id: fileRow
                    required property var modelData
                    width: fileList.width
                    spacing: Theme.space2

                    StatusBadge {
                        text: fileRow.modelData.action === "overwrite" ? qsTr("Substituir")
                            : fileRow.modelData.action === "create" ? qsTr("Criar")
                            : fileRow.modelData.action === "unchanged" ? qsTr("Igual")
                            : qsTr("Pular")
                        kind: fileRow.modelData.action === "overwrite" && fileRow.modelData.newer ? StatusBadge.Warning
                            : StatusBadge.Neutral
                    }
                    Text {
                        Layout.fillWidth: true
                        text: fileRow.modelData.name
                        color: Theme.text
                        font.family: Theme.fontBody
                        font.pixelSize: Theme.textMeta
                        elide: Text.ElideMiddle
                        ToolTip.visible: fileHover.hovered
                        ToolTip.text: fileRow.modelData.target + (fileRow.modelData.reason ? "\n" + fileRow.modelData.reason : "")
                        HoverHandler { id: fileHover }
                    }
                    Text {
                        text: fileRow.modelData.action === "overwrite"
                              ? fileRow.modelData.currentSize + " → " + fileRow.modelData.backupSize
                                + (fileRow.modelData.newer ? "  " + qsTr("(o atual é mais novo)") : "")
                              : fileRow.modelData.backupSize
                        color: fileRow.modelData.newer ? Theme.warningFg : Theme.textSubtle
                        font.family: Theme.fontBody
                        font.pixelSize: Theme.textMeta
                    }
                }
            }
        }

        Repeater {
            model: [
                { show: root.plan.newer === true && !(root.switching && (root.plan.activeSlot || "").length > 0),
                  text: qsTr("Alguns arquivos atuais são mais novos que este backup. Você pode perder progresso recente.") },
                { show: root.plan.gameRunning === true, text: qsTr("O jogo está aberto. Feche-o para restaurar.") }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                visible: modelData.show
                implicitHeight: warningText.implicitHeight + 2 * Theme.space3
                radius: Theme.radiusControl
                color: Theme.warningBg

                Text {
                    id: warningText
                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    text: parent.modelData.text
                    color: Theme.warningFg
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.plan.cloudConflict === true && root.plan.hasChanges === true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                text: qsTr("O cloud do launcher também sincroniza estes saves e pode sobrescrever a restauração. Quem deve ter prioridade?")
                color: Theme.text
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }
            RowLayout {
                spacing: Theme.space2
                ButtonGroup { id: cloudGroup }
                FilterChip {
                    text: qsTr("SaveForge: restaurar agora")
                    checked: root.cloudChoice === "saveforge"
                    ButtonGroup.group: cloudGroup
                    onClicked: root.cloudChoice = "saveforge"
                }
                FilterChip {
                    text: qsTr("Launcher: não restaurar")
                    checked: root.cloudChoice === "launcher"
                    ButtonGroup.group: cloudGroup
                    onClicked: root.cloudChoice = "launcher"
                }
            }
            Text {
                Layout.fillWidth: true
                visible: root.cloudChoice === "saveforge"
                text: qsTr("Depois de restaurar, abra o jogo e, se o launcher avisar sobre conflito, escolha manter os arquivos locais.")
                color: Theme.textSubtle
                font.family: Theme.fontBody
                font.pixelSize: Theme.textMeta
                wrapMode: Text.Wrap
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.plan.hasChanges === true
            text: !root.switching
                  ? qsTr("Antes de restaurar, os arquivos atuais serão guardados no histórico como “Antes de restaurar”, para que você possa desfazer.")
                  : (root.plan.activeSlot || "").length > 0
                    ? qsTr("O progresso atual fica guardado no save “%1”, e você pode voltar a ele quando quiser.").arg(root.plan.activeSlot)
                    : qsTr("O progresso atual não tem nome: ele fica só no histórico, como “Antes de restaurar”. Para alternar com ele depois, cancele e use “Salvar como novo save” primeiro.")
            color: Theme.textSubtle
            font.family: Theme.fontBody
            font.pixelSize: Theme.textMeta
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            spacing: Theme.space3
            Item { Layout.fillWidth: true }
            SecondaryButton {
                text: qsTr("Cancelar")
                onClicked: root.close()
            }
            PrimaryButton {
                text: root.switching ? qsTr("Trocar de save") : qsTr("Restaurar")
                enabled: root.canRestore && !AppController.busy
                onClicked: {
                    AppController.restore(root.gameId, root.snapshot.id, "prefer-saveforge")
                    root.close()
                }
            }
        }
    }
}
