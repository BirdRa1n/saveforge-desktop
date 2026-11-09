pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

// Shows what a restore would do and asks for confirmation. Nothing is written
// before the user confirms; the controller then executes the reviewed plan.
Popup {
    id: root

    property string gameId
    property var snapshot: ({})
    property var plan: ({})
    // "saveforge" or "launcher" when the launcher's cloud also syncs the saves.
    property string cloudChoice: ""

    readonly property bool canRestore: plan.ok === true && plan.hasChanges === true && plan.gameRunning !== true
                                       && (plan.cloudConflict !== true || cloudChoice === "saveforge")

    function openFor(entry) {
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
            text: qsTr("Restaurar backup de %1").arg(root.snapshot.date || "")
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
            color: "#F87171"
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
                    { n: root.plan.extra || 0, one: qsTr("arquivo atual não está no backup e será mantido"), many: qsTr("arquivos atuais não estão no backup e serão mantidos") }
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
                text: qsTr("Os arquivos atuais já são iguais a este backup. Não há nada para restaurar.")
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: Theme.textBody
                wrapMode: Text.Wrap
            }
        }

        Repeater {
            model: [
                { show: root.plan.newer === true, text: qsTr("Alguns arquivos atuais são mais novos que este backup. Você pode perder progresso recente.") },
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
            text: qsTr("Antes de restaurar, os arquivos atuais serão guardados no histórico como “Antes de restaurar”, para que você possa desfazer.")
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
                text: qsTr("Restaurar")
                enabled: root.canRestore && !AppController.busy
                onClicked: {
                    AppController.restore(root.gameId, root.snapshot.id, "prefer-saveforge")
                    root.close()
                }
            }
        }
    }
}
