import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SaveForge.Backend

ApplicationWindow {
    id: window

    // Game shown in the detail page; empty shows the selected sidebar page.
    property string selectedGameId

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "SaveForge"
    color: Theme.bg

    font.family: Theme.fontBody
    font.pixelSize: Theme.textBody

    Connections {
        target: AppController
        function onNotify(text, kind) { toast.show(text, kind) }
        function onLargeBackupFound(gameId, name, size, expected) {
            largeBackupDialog.gameId = gameId
            largeBackupDialog.text = qsTr("Os saves de %1 têm %2, mais do que o perfil do jogo espera (%3). Isso pode indicar arquivos que não são saves. Fazer o backup mesmo assim?")
                                     .arg(name).arg(size).arg(expected)
            largeBackupDialog.open()
        }
    }

    // Confirms a backup larger than the game's profile expects (backup.maxBytes).
    Popup {
        id: largeBackupDialog
        objectName: "largeBackupDialog"
        property string gameId
        property string text
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
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

        contentItem: ColumnLayout {
            spacing: Theme.space4
            Text {
                Layout.fillWidth: true
                text: qsTr("Backup maior que o esperado")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.textSectionTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: largeBackupDialog.text
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
                    onClicked: largeBackupDialog.close()
                }
                PrimaryButton {
                    text: qsTr("Fazer backup")
                    onClicked: {
                        largeBackupDialog.close()
                        AppController.backupGameAnyway(largeBackupDialog.gameId)
                    }
                }
            }
        }
    }

    // With a tray icon and the user's consent, closing only hides the window
    // so automatic backups keep running; otherwise it quits the app.
    onClosing: close => {
        if (AppController.trayAvailable && AppController.runInBackground) {
            close.accepted = false
            window.hide()
        } else {
            Qt.quit()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            objectName: "sidebar"
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            vaultPath: AppController.vaultReady ? AppController.vaultPath : qsTr("Indisponível")
            statusText: AppController.busy ? AppController.busyText
                                           : Format.count(AppController.library.count, qsTr("jogo"), qsTr("jogos")) + qsTr(" · funciona offline")
            onCurrentIndexChanged: window.selectedGameId = ""
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.selectedGameId.length > 0 ? 4 : sidebar.currentIndex

            LibraryPage {
                objectName: "libraryPage"
                onGameSelected: gameId => window.selectedGameId = gameId
                onOpenSettings: sidebar.currentIndex = 3
            }
            ActivityPage {
                objectName: "activityPage"
            }
            TransferPage {
                objectName: "transferPage"
            }
            SettingsPage {
                objectName: "settingsPage"
            }
            GameDetailPage {
                objectName: "gameDetailPage"
                gameId: window.selectedGameId
                onBack: window.selectedGameId = ""
            }
        }
    }

    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space6
    }
}
