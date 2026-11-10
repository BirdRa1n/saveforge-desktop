import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveVault.Backend

// Names the progress on disk as a new save slot ("salvar como"): backs up the
// current files into the slot and makes it the active one.
Popup {
    id: root

    property string gameId
    property string error: ""

    function openEmpty() {
        nameField.text = ""
        error = ""
        open()
        nameField.forceActiveFocus()
    }

    function submit() {
        error = AppController.saveAsSlot(gameId, nameField.text)
        if (error.length === 0)
            close()
    }

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
            text: qsTr("Salvar como novo save")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textSectionTitle
            font.weight: Font.Bold
            Accessible.role: Accessible.Heading
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("O progresso atual vira um save com nome, como “Segundo personagem” ou “Run hardcore”. Você pode trocar entre os saves quando quiser, e cada um continua de onde parou.")
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            wrapMode: Text.Wrap
        }

        FormField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: qsTr("Nome do save")
            maximumLength: 100
            onTextEdited: root.error = ""
            onAccepted: root.submit()
        }

        Text {
            Layout.fillWidth: true
            visible: root.error.length > 0
            text: root.error
            color: Theme.warningFg
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
                onClicked: root.close()
            }
            PrimaryButton {
                text: qsTr("Salvar")
                enabled: nameField.text.trim().length > 0 && !AppController.busy && AppController.vaultReady
                onClicked: root.submit()
            }
        }
    }
}
