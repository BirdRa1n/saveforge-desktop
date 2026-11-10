pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SaveVault.Backend

// Adds a game the profiles do not cover: a name plus the folders where it
// keeps its saves. The controller turns them into a local profile.
Popup {
    id: root

    property var folders: []
    property string error: ""

    function openEmpty() {
        nameField.text = ""
        folders = []
        error = ""
        open()
        nameField.forceActiveFocus()
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    focus: true
    width: 560
    padding: Theme.space6
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.surface
        border.color: Theme.borderStrong
    }

    Overlay.modal: Rectangle { color: Theme.scrim }

    FolderDialog {
        id: folderDialog
        title: qsTr("Escolha a pasta de saves")
        onAccepted: {
            const chosen = selectedFolder.toString()
            if (root.folders.indexOf(chosen) < 0)
                root.folders = root.folders.concat([chosen])
            root.error = ""
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space4

        Text {
            Layout.fillWidth: true
            text: qsTr("Adicionar jogo")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.textSectionTitle
            font.weight: Font.Bold
            Accessible.role: Accessible.Heading
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Para jogos que o SaveVault ainda não conhece. Informe o nome e as pastas onde o jogo guarda os saves.")
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: Theme.textBody
            wrapMode: Text.Wrap
        }

        FormField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: qsTr("Nome do jogo")
            onTextEdited: root.error = ""
        }

        Repeater {
            model: root.folders
            delegate: RowLayout {
                id: folderRow
                required property string modelData
                required property int index
                Layout.fillWidth: true
                spacing: Theme.space2

                Icon { path: Icons.folder; color: Theme.textMuted }
                Text {
                    Layout.fillWidth: true
                    text: decodeURIComponent(folderRow.modelData.replace(/^file:\/{2,3}/, ""))
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: Theme.textBody
                    elide: Text.ElideMiddle
                }
                SecondaryButton {
                    text: qsTr("Remover")
                    Accessible.name: qsTr("Remover pasta %1").arg(folderRow.modelData)
                    onClicked: {
                        const copy = root.folders.slice()
                        copy.splice(folderRow.index, 1)
                        root.folders = copy
                    }
                }
            }
        }

        SecondaryButton {
            text: root.folders.length === 0 ? qsTr("Escolher pasta de saves…") : qsTr("Adicionar outra pasta…")
            onClicked: folderDialog.open()
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
                text: qsTr("Adicionar")
                enabled: nameField.text.trim().length > 0 && root.folders.length > 0 && !AppController.busy
                onClicked: {
                    root.error = AppController.addManualGame(nameField.text, root.folders)
                    if (root.error.length === 0)
                        root.close()
                }
            }
        }
    }
}
